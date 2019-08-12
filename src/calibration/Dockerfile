FROM {{base_image_name}}:{{base_image_tag}}
MAINTAINER Jiacheng Yao "jiacheng@pik-potsdam.de"

USER root

ADD .bashrc /root/.bashrc
ADD .aliases /root/.aliases
ADD .gitattributes /root/.gitattributes
ADD .gitconfig /root/.gitconfig
ADD .gitignore /root/.gitignore

RUN echo nameserver 8.8.8.8 > /etc/resolv.conf

# apt-gets
RUN apt-get update
RUN apt-get install -y libeigen3-dev libboost-serialization-dev libboost-filesystem-dev libboost-test-dev libboost-program-options-dev libboost-thread-dev libtbb-dev

ADD supervisord.conf /etc/supervisor/conf.d/supervisord.conf

# prep init
ADD init.sh /usr/local/bin/init.sh
# COPY init.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/init.sh
CMD ["init.sh"]

