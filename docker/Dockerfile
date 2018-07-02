# Build a docker image from github source code

FROM ubuntu:16.04
MAINTAINER walle x64 <wallex64@outlook.com>

RUN apt-get update && apt-get upgrade -y
RUN apt-get install -y git build-essential libboost-all-dev libssl-dev cmake autoconf

RUN cd /root && git clone https://github.com/pidiscovery/pi.git
RUN cd /root/pi && git submodule update --init --recursive 
RUN cd /root/pi && mkdir build && cd build && cmake .. 
RUN cd /root/pi/build && make install
RUN rm -rf /root/pi

ADD entry.sh /entry.sh

EXPOSE 40010/tcp
EXPOSE 8010/tcp

CMD ["/entry.sh"]
