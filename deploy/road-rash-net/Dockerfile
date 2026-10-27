# rr-netd is compiled for Linux amd64 in CI using server/Dockerfile.
# This archive/image contains code only. Game assets never enter either.
FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends libssl3 ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --home /data --create-home rrnet
COPY rr-netd /usr/local/bin/rr-netd
USER rrnet
WORKDIR /data
ENV RR_NETD_PORT=8080 RR_NETD_HTTP_PORT=8081 RR_NETD_LOG_DIR=/data/race-logs
EXPOSE 8080
EXPOSE 8081
CMD ["/usr/local/bin/rr-netd"]
