FROM gcc:latest

WORKDIR /app
COPY main.c .

RUN gcc -o loadbalancer main.c

CMD ["./loadbalancer"]
