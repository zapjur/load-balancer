FROM gcc:latest

WORKDIR /app
COPY main.c .
COPY balancer.c .
COPY balancer.h .
COPY metrics.c .
COPY backends.conf .

RUN gcc -o loadbalancer main.c balancer.c metrics.c -lpthread

CMD ["./loadbalancer"]
