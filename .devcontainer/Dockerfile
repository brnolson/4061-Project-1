FROM ubuntu:22.04

RUN yes | unminimize

RUN apt update && apt install -y \
    build-essential \
    curl \
    file \
    gdb \
    git \
    man \
    python3 \
    screen \
    valgrind \
    wget \
    zip

# Ubuntu 22.04 default clang-format package is too old
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-19 main" >> \
    /etc/apt/sources.list
RUN echo "deb-src http://apt.llvm.org/jammy/ llvm-toolchain-jammy-19 main" >> \
    /etc/apt/sources.list

RUN apt update && apt install -y clang-format-19
RUN ln -s /usr/bin/clang-format-19 /usr/bin/clang-format
