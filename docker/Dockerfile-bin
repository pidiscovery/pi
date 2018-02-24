FROM ubuntu:14.04
MAINTAINER "wallex64 wallex64@outlook.com"

ADD entry.sh /
ADD witness_node /usr/local/bin
ADD cli_wallet /usr/local/bin


EXPOSE 40010/tcp
EXPOSE 8010/tcp

CMD ["/entry.sh"]

