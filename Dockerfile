FROM quay.io/pypa/manylinux2014_x86_64

RUN yum -y update && yum -y install boost-devel && yum clean all

ENV PATH="/opt/python/cp38-cp38/bin/:$PATH"
