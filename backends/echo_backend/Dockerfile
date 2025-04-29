FROM gcc:latest

WORKDIR /app
COPY echo_backend.c .

RUN gcc -o echo_backend echo_backend.c

CMD ["./echo_backend", "9003"]
