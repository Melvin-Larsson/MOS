FROM joshwyant/gcc-cross AS base

RUN ["apt-get", "update"]
RUN ["apt-get", "install", "make"]


RUN ["cp", "/usr/local/cross/bin/i686-elf-gcc", "."]
RUN ["rm", "-r", "/usr/local/cross/bin"]
RUN ["mkdir", "/usr/local/cross/bin"]
RUN ["mv", "i686-elf-gcc", "/usr/local/cross/bin"]


VOLUME ["/src"]

WORKDIR "/src"

# COPY ["sysroot", "sysroot"]
# COPY ["tests", "tests"]
# COPY ["utils", "utils"]
# COPY ["makefile", "."]
# COPY ["linker.ld", "."]

ENV COMPILER="/usr/local/cross/bin/i686-elf-gcc"
ENV PREFIX="/src"

ENTRYPOINT ["make"]
