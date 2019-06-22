FROM gravity-ubuntu-base

RUN apt install -y libssl-dev

ENV OPENSSL_ENABLED=true

ENTRYPOINT [ "/bin/bash" ]
CMD [ "-c", "./examples/http/run.sh" ]