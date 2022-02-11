# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FROM python:3.6.5


LABEL Description="Builds the KenLM library for estimating language models"

RUN apt-get update && \
    apt-get install -y \
            cmake \
            git \
            g++ \
            libboost-all-dev \
            libbz2-dev \
            libeigen3-dev \
            liblzma-dev \            
            libz-dev \
            make \
            curl

RUN curl -L https://kheafield.com/code/kenlm.tar.gz | tar -xzvf -

WORKDIR /kenlm
RUN mkdir -p build
WORKDIR build
RUN cmake ..
RUN make -j 4
WORKDIR /kenlm

# Optional python package. Comment if not required
RUN pip install https://github.com/kpu/kenlm/archive/master.zip
