# Plan 34 Task 5/8: rr-netd image. Build context is the repository root (or
# the deploy archive, which holds only cmake/net.cmake, src/net and server):
#   docker build -f server/Dockerfile --build-arg RR_NETD_REVISION=$(git rev-parse HEAD) .
FROM debian:bookworm-slim AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ cmake make git ca-certificates libssl-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY cmake/net.cmake cmake/net.cmake
COPY src/net src/net
COPY server server
ARG RR_NETD_REVISION=unknown
RUN cmake -S server -B /build -DCMAKE_BUILD_TYPE=Release -DRR_NETD_REVISION="${RR_NETD_REVISION}" \
 && cmake --build /build --target rr-netd -j"$(nproc)" \
 && strip /build/rr-netd

FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends libssl3 ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --home /data --create-home rrnet
COPY --from=build /build/rr-netd /usr/local/bin/rr-netd
USER rrnet
WORKDIR /data
ENV RR_NETD_PORT=8080 RR_NETD_HTTP_PORT=8081 RR_NETD_LOG_DIR=/data/race-logs
EXPOSE 8080 8081 40000-40099/udp
CMD ["/usr/local/bin/rr-netd"]
