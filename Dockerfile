FROM ubuntu:18.04

RUN apt-get update &&\
  apt-get install -y sudo time git-core vim subversion build-essential gcc-multilib \
  libncurses5-dev zlib1g-dev gawk flex gettext wget unzip python &&\
  apt-get clean

RUN useradd -m openwrt &&\
  echo 'openwrt ALL=NOPASSWD: ALL' > /etc/sudoers.d/openwrt

USER openwrt
WORKDIR /home/openwrt

RUN git clone https://github.com/LibreRouterOrg/openwrt.git &&\
  openwrt/scripts/feeds update -a &&\
  openwrt/scripts/feeds install -a &&\
  cp openwrt/configs/default_config openwrt/.config