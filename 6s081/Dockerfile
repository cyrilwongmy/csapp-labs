FROM ubuntu:20.04


# Set environment variables to avoid user interaction during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update the package list and install dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        net-tools \
        curl \
        sudo \
        wget \
        locales \
        git \
        vim \
        python3 \
        python3-pip \
        zsh \
        gdb-multiarch \
        qemu-system-misc \
        gcc-riscv64-linux-gnu \
        binutils-riscv64-linux-gnu \
        # Add other dependencies you need here \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m s081 && \
    echo "root ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers && \
    echo "s081 ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
RUN locale-gen en_US.UTF-8

RUN mkdir -p /home/s081/project

ENV LANG=en_US.utf8 \
    LANGUAGE=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8

RUN chsh -s $(which zsh)

RUN usermod -s /bin/zsh s081
RUN chsh -s /bin/zsh s081
ENV SHELL=/bin/zsh 

ENV PASSWORD=s081

USER s081
ENV PASSWORD=s081

WORKDIR /home/s081/project

