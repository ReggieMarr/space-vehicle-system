# Upgrade to Ubuntu 24.04 as recommended
FROM ubuntu:22.04 AS fprime_deps

# Set non-interactive installation mode for apt packages
ARG DEBIAN_FRONTEND=noninteractive

# Install all necessary packages in one layer to reduce intermediate layers
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y ssh clang-format sudo build-essential git cmake python3 python3-venv python3-pip python3-wheel \
    python3-dev python3-tk wget gdbserver openssh-server iputils-ping netcat libicu-dev rsync

# Download, build and install Boost with static libraries
RUN wget -O boost.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.gz && \
    tar xzf boost.tar.gz && \
    mv boost_1_78_0 /opt/boost && \
    cd /opt/boost && \
    ./bootstrap.sh --with-libraries=system,filesystem && \
    ./b2 link=static install && cd /

# Set the working directory for fprime software
ARG FSW_WDIR=/fsw

# Create a non-root user for better security practices
ARG HOST_UID=1000
ARG HOST_GID=1000
ARG GIT_COMMIT
ARG GIT_BRANCH
RUN groupadd -g $HOST_GID user && \
    useradd -u $HOST_UID -g $HOST_GID -m user && \
    echo 'user ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

WORKDIR $FSW_WDIR
RUN chown -R user:user $FSW_WDIR
USER user

FROM fprime_deps AS fprime_src

# Clone the repository
RUN git clone https://github.com/ReggieMarr/MBSE_FSW.git $FSW_WDIR
RUN git fetch
RUN git checkout $GIT_BRANCH
RUN git reset --hard $GIT_COMMIT

# Update submodule URLs to use HTTPS with access token
# NOTE this is a bit hacky, could clean up

# Set up Git configuration
RUN git config --global url."https://github.com/".insteadOf "git@github.com:"

# Update .gitmodules file
RUN sed -i 's|git@github.com:|https://github.com/|g' .gitmodules

# Sync and update submodules
RUN git submodule sync --recursive

# Update nested .gitmodules files
RUN git submodule foreach --recursive '\
    if [ -f .gitmodules ]; then \
        sed -i "s|git@github.com:|https://github.com/|g" .gitmodules; \
        git submodule sync --recursive; \
    fi \
    '
# Sync the updated submodule configurations
RUN git submodule sync --recursive

RUN git submodule update --init --recursive --depth 1 --recommend-shallow
ENV FPRIME_FRAMEWORK_PATH=$FSW_WDIR/fprime

# Setup environment
ENV PATH="/home/user/.local/bin:${PATH}"
# Install Python packages
RUN pip install setuptools setuptools_scm wheel pip fprime-tools --upgrade && \
    pip install -r $FSW_WDIR/fprime/requirements.txt && \
    # pip install -e $FSW_WDIR/fprime-gds && \
    pip install pytest debugpy pyinfra asyncio asyncssh gitpython python-dotenv --upgrade

FROM fprime_src AS stars_base

ENV TZ='America/Montreal'

# Ubuntu packages
USER root
RUN echo $TZ > /etc/timezone && \
    apt-get update && apt-get install -y --no-install-recommends tzdata && \
    rm /etc/localtime && \
    ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && \
    dpkg-reconfigure -f noninteractive tzdata && \
    dpkg --add-architecture i386 && apt-get update && \
    apt-get install -y --no-install-recommends cmake wget gcc g++ less vim nano git curl make csh time python2 python3-pip python3-venv build-essential gcovr && \
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ENV PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:${PATH}"
# Set library path
ENV LD_LIBRARY_PATH=/usr/local/lib

USER user

# This seems to be where fprime expects STARS to be
RUN git clone https://github.com/JPLOpenSource/STARS.git ${HOME}/STARS
RUN pip install -r ${HOME}/STARS/requirements.txt

# CCSDS testing
RUN pip install spacepackets
