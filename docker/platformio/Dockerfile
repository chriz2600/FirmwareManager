FROM debian:stretch-backports

# basic packages
RUN apt-get update && \
    apt-get -y install git emacs24-nox python2.7 python-pip locales

# Set LOCALE to UTF8
RUN echo "en_US.UTF-8 UTF-8" > /etc/locale.gen && \
    locale-gen en_US.UTF-8 && \
    update-locale LANG=en_US.UTF-8

RUN pip install -U platformio
