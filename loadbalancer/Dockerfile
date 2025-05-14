FROM gcc:latest

WORKDIR /app
COPY main.c .
COPY balancer.c .
COPY balancer.h .

RUN gcc -o loadbalancer main.c balancer.c

CMD ["./loadbalancer"]
