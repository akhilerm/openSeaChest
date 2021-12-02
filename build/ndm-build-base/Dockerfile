FROM debian:buster as build
RUN apt-get update && apt-get install -y make git gcc
WORKDIR /tmp/openSeachest
RUN git clone --recursive --branch Release-19.06.02 https://github.com/openebs/openSeaChest.git .
WORKDIR /tmp/openSeachest/Make/gcc
RUN make release
WORKDIR /tmp/openSeachest
RUN cp opensea-common/Make/gcc/lib/libopensea-common.a /usr/lib && \
    cp opensea-operations/Make/gcc/lib/libopensea-operations.a /usr/lib && \
    cp opensea-transport/Make/gcc/lib/libopensea-transport.a /usr/lib && \
    # Copy include headers
    cp -r include /usr/local/include/openSeaChest && \
    cp -r opensea-common/include /usr/local/include/openSeaChest/opensea-common && \
    cp -r opensea-operations/include /usr/local/include/openSeaChest/opensea-operations && \
    cp -r opensea-transport/include /usr/local/include/openSeaChest/opensea-transport

FROM golang:1.14.7
RUN apt-get update && apt-get install -y make git libudev-dev libblkid-dev
COPY --from=build /usr/lib/libopensea-* /usr/lib/
COPY --from=build /usr/local/include /usr/local/include/
WORKDIR /go/src/github.com/openebs/node-disk-manager/
ARG DBUILD_DATE
ARG DBUILD_REPO_URL="https://github.com/openebs/openSeaChest"
ARG DBUILD_SITE_URL="http://www.openebs.io/"

LABEL org.label-schema.schema-version="1.0"
LABEL org.label-schema.name="ndm-base-build"
LABEL org.label-schema.description="Base image for building ndm binaries"
LABEL org.label-schema.build-date=$DBUILD_DATE
LABEL org.label-schema.vcs-url=$DBUILD_REPO_URL
LABEL org.label-schema.url=$DBUILD_SITE_URL
