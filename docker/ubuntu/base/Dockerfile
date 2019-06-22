FROM ubuntu:latest

RUN apt update
RUN apt install -y make pkg-config

COPY . /app
WORKDIR /app
ENV PATH="/app:${PATH}"