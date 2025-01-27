# Upgrade to Ubuntu 24.04 as recommended
FROM ubuntu:24.04 AS fprime_deps
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ='America/Montreal'

# Install all necessary packages in one layer to reduce intermediate layers
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y python3-dev python3 python3-full python3-pip python3-wheel python3-venv \
    wget gdbserver libicu-dev rsync clang-format sudo build-essential git cmake

# Download, build and install Boost with static libraries
RUN wget -O boost.tar.gz https://pilotfiber.dl.sourceforge.net/project/boost/boost/1.78.0/boost_1_78_0.tar.gz?viasf=1 && \
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
RUN userdel -r ubuntu || true && \
    getent group 1000 && getent group 1000 | cut -d: -f1 | xargs groupdel || true && \
    groupadd -g ${HOST_GID} user && \
    useradd -u ${HOST_UID} -g ${HOST_GID} -m user && \
    echo 'user ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

WORKDIR $FSW_WDIR
RUN chown -R user:user $FSW_WDIR
USER user
ENV TZ='America/Montreal'

RUN mkdir /home/user/ninja
RUN wget -qO /home/user/ninja/ninja.gz https://github.com/ninja-build/ninja/releases/latest/download/ninja-linux.zip -nv
RUN gunzip /home/user/ninja/ninja.gz
RUN chmod a+x /home/user/ninja/ninja

ENV PATH="/home/user/ninja/:$HOME/cmsis-toolbox-linux-amd64/bin:$PATH"

ENV CMAKE_MAKE_PROGRAM=ninja

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
ENV VIRTUAL_ENV=/home/user/venv
RUN python3 -m venv $VIRTUAL_ENV
ENV PATH="/home/user/.local/bin:$VIRTUAL_ENV/bin:$PATH"

# Activate virtual environment in various shell initialization files
RUN echo "source $VIRTUAL_ENV/bin/activate" >> ~/.bashrc && \
    echo "source $VIRTUAL_ENV/bin/activate" >> ~/.profile

# Upgrade pip in virtual environment
RUN pip install --upgrade pip

# Install Python packages
RUN pip install setuptools setuptools_scm wheel pip fprime-tools --upgrade && \
    pip install -r $FSW_WDIR/fprime/requirements.txt && \
    # pip install -e $FSW_WDIR/fprime-gds && \
    pip install pytest debugpy pyinfra asyncio asyncssh gitpython python-dotenv --upgrade

FROM fprime_src AS stars_base

# Ubuntu packages
# USER root
# RUN echo $TZ > /etc/timezone && \
#     apt-get update && apt-get install -y --no-install-recommends tzdata && \
#     rm /etc/localtime && \
#     ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && \
#     dpkg-reconfigure -f noninteractive tzdata && \
#     dpkg --add-architecture i386 && apt-get update && \
#     apt-get install -y --no-install-recommends cmake wget gcc g++ less vim nano git curl make csh time python2 python3-pip python3-venv build-essential gcovr && \
#     apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# ENV PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:${PATH}"
# ENV LD_LIBRARY_PATH=/usr/local/lib

USER user

# This seems to be where fprime expects STARS to be
RUN git clone https://github.com/JPLOpenSource/STARS.git /home/user/STARS
RUN source /home/user/STARS/venv/bin/activate && \
    pip install -r ${HOME}/STARS/requirements.txt && \
    pip install anytree --upgrade

# # CCSDS testing
# RUN pip install spacepackets

FROM fprime_src AS fprime_cleanup
USER root
RUN apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
USER user
