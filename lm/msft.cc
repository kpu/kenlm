#include "util/string_piece.hh"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <err.h>

class MicrosoftFail : public std::exception {
  public:
    virtual const char *what() const throw() {
      return message_.c_str();
    }
    
  protected:
    MicrosoftFail() throw() {}
    virtual ~MicrosoftFail() throw() {}

    std::string message_;
};

class MicrosoftHTTPCode : public MicrosoftFail {
  public:
    explicit MicrosoftHTTPCode(const std::string &status) throw() : status_(status) {
      message_ = "Microsoft returned status '";
      message_ += status_;
      message_ += "'";
    }

    ~MicrosoftHTTPCode() throw() {}

  private:
    std::string status_;
};

class MicrosoftMissingLength : public MicrosoftFail {
  public:
    MicrosoftMissingLength() throw() {
      message_ = "Missing Content-Length: header";
    }
    ~MicrosoftMissingLength() throw() {}
};

class MicrosoftQuery {
  public:
    MicrosoftQuery(const std::string &plural_method, const std::string &auth_token, const std::string &urn) :
      plural_method_(plural_method), auth_token_(auth_token), urn_(urn) {}

    void Run(const std::vector<std::string> &phrases, std::vector<float> &probs) {
      boost::asio::ip::tcp::iostream server("web-ngram.research.microsoft.com", "http");
      if (!server) {
        err(1, "Server connection");
      }
      server.exceptions(std::istream::eofbit | std::istream::failbit | std::istream::badbit);
      MakeRequest(phrases, server);
      ParseResponse(server, phrases.size(), probs);
    }

  private:
    void MakeXML(const std::vector<std::string> &phrases, std::ostream &out) const;
    void MakeRequest(const std::vector<std::string> &phrases, std::ostream &out) const;

    void ParseResponse(std::istream &in, size_t expected, std::vector<float> &out) const;

    std::string plural_method_;
    std::string auth_token_;
    std::string urn_;
};

void EscapeToOStream(const StringPiece &str, std::ostream &out) {
  for (const char *i = str.data(); i != str.data() + str.length(); ++i) {
    if ((0 < *i && *i < 32) || *i == 127) {
      unsigned int tmp = *i;
      out << "&#" << tmp << ';';
      continue;
    }
    switch (*i) {
      case '<': 
        out << "&lt;";
        break;
      case '>':
        out << "&gt;";
        break;
      case '"':
        out << "&quot;";
        break;
      case '&':
        out << "&amp;";
        break;
      case '\'':
        out << "&apos;";
        break;
      default:
        out.put(*i);
    }
  }
}

void MicrosoftQuery::MakeXML(const std::vector<std::string> &words, std::ostream &out) const {
  out << 
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:w=\"http://www.w3.org/2005/08/addressing\">"
    "<s:Header>"
    "<w:To s:mustUnderstand=\"1\">http://web-ngram.research.microsoft.com/Lookup.svc</w:To>"
    "<w:Action s:mustUnderstand=\"1\">http://schemas.microsoft.com/research/2009/10/webngram/frontend/ILookupService/" << plural_method_ << "</w:Action>"
    "</s:Header>"
    "<s:Body>"
    "<" << plural_method_ << " xmlns=\"http://schemas.microsoft.com/research/2009/10/webngram/frontend\">"
    "<authorizationToken>" << auth_token_ << "</authorizationToken>"
    "<modelUrn>" << urn_ << "</modelUrn>"
    "<phrases xmlns:a=\"http://schemas.microsoft.com/2003/10/Serialization/Arrays\">";
  for (std::vector<std::string>::const_iterator i = words.begin(); i != words.end(); ++i) {
    out << "<a:string>";
    EscapeToOStream(*i, out);
    out << "</a:string>";
  }
  out <<
    "</phrases>"
    "</" << plural_method_ << ">"
    "</s:Body>"
    "</s:Envelope>";
}

void MicrosoftQuery::MakeRequest(const std::vector<std::string> &phrases, std::ostream &out) const {
  std::stringstream buffer;
  MakeXML(phrases, buffer);
  std::string made(buffer.str());
  out << 
    "POST /Lookup.svc HTTP/1.1\n"
    "User-Agent: kheafiel\n"
    "Host: web-ngram.research.microsoft.com\n"
    "Accept: */*\n"
    "Content-Type: application/soap+xml; charset=utf-8\n"
    "Content-Length: " << made.size() << "\n\n";
  out << made;
}

namespace {

class UnexpectedInput : public std::exception {
  public:
    explicit UnexpectedInput(const StringPiece &expected, size_t offset, char got) throw() {
      message_ = "Expected '";
      message_.append(expected.data(), expected.size());
      message_ += "', but got '";
      message_.append(expected.data(), offset);
      message_ += got;
      message_ += "' with first disagreement on the last character";
    }

    ~UnexpectedInput() throw() {}

    const char *what() throw() { return message_.c_str(); }

  private:
    std::string message_;
};

void ExpectIStream(std::istream &in, const StringPiece &text) {
  std::ios_base::iostate state = in.exceptions();
  in.exceptions(std::istream::eofbit | std::istream::failbit | std::istream::badbit);
  char got;
  for (const char *i = text.data(); i != text.data() + text.length(); ++i) {
    in.get(got);
    if (got != *i) {
      in.exceptions(state);
      throw UnexpectedInput(text, i - text.data(), got);
    }
  }
  in.exceptions(state);
}
} // namespace

void MicrosoftQuery::ParseResponse(std::istream &in, size_t expected, std::vector<float> &out) const {
  std::string line;
  getline(in, line);
  if (line != "HTTP/1.1 200 OK\r") throw MicrosoftHTTPCode(line);
  const char length_prefix[] = "Content-Length: ";
  size_t length = 0;
  while (getline(in, line) && line != "\r" && !line.empty()) {
    if (!strncmp(line.c_str(), length_prefix, strlen(length_prefix))) {
      line.resize(line.size() - 1);
      length = boost::lexical_cast<size_t>(line.c_str() + strlen(length_prefix));
    }
  }
  if (!length) throw MicrosoftMissingLength();
  ExpectIStream(in, "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://www.w3.org/2005/08/addressing\"><s:Header><a:Action s:mustUnderstand=\"1\">http://schemas.microsoft.com/research/2009/10/webngram/frontend/ILookupService/");
  ExpectIStream(in, plural_method_);
  ExpectIStream(in, "Response</a:Action></s:Header><s:Body><");
  ExpectIStream(in, plural_method_);
  ExpectIStream(in, "Response xmlns=\"http://schemas.microsoft.com/research/2009/10/webngram/frontend\"><");
  ExpectIStream(in, plural_method_);
  ExpectIStream(in, "Result xmlns:b=\"http://schemas.microsoft.com/2003/10/Serialization/Arrays\" xmlns:i=\"http://www.w3.org/2001/XMLSchema-instance\">");
  out.clear();
  out.reserve(expected);
  for (size_t i = 0; i < expected; ++i) {
    ExpectIStream(in, "<b:float>");
    float tmp;
    in >> tmp;
    out.push_back(tmp);
    ExpectIStream(in, "</b:float>");
  }
  ExpectIStream(in, "</");
  ExpectIStream(in, plural_method_);
  ExpectIStream(in, "Result></");
  ExpectIStream(in, plural_method_);
  ExpectIStream(in, "Response></s:Body></s:Envelope>");
}

int main() {
  MicrosoftQuery q("GetProbabilities", "1dfc6983-83c5-41a0-8572-a8c3579bf838", "urn:ngram:bing-body:jun09:3");
  std::vector<std::string> queries(1);
  while (getline(std::cin, queries.back())) {
    queries.resize(queries.size() + 1);
  }
  queries.resize(queries.size() - 1);
  std::vector<float> probs;
  q.Run(queries, probs);
  std::copy(probs.begin(), probs.end(), std::ostream_iterator<float>(std::cout, "\n"));
}
