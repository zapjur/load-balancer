FROM gcc:latest

WORKDIR /app
COPY simple_backend.c .

RUN gcc -o simple_backend simple_backend.c

CMD ["./simple_backend", "9001"]
