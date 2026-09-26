# Build stage
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy CMakeLists.txt first for better caching
COPY CMakeLists.txt .

# Pre-fetch dependencies
RUN cmake -B build -DHYPERCACHE_BUILD_SERVER=ON -DHYPERCACHE_BUILD_REDIS=ON \
    && cmake --build build --target hypercache_server --config Release -j$(nproc) 2>&1 | tail -20 || true

# Copy source and build
COPY include/ include/
COPY src/ src/

RUN cmake --build build --target hypercache_server --config Release -j$(nproc)

# Runtime stage
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -r -m -s /bin/bash appuser

WORKDIR /app

COPY --from=builder /build/build/hypercache_server .

RUN chown appuser:appuser hypercache_server

USER appuser

EXPOSE 18080

ENV CACHE_TYPE=lru \
    REDIS_HOST=redis \
    REDIS_PORT=6379 \
    REDIS_POOL_MIN=2 \
    REDIS_POOL_MAX=10

# The server reads the env vars above itself; extra CLI flags passed to
# `docker run` still override them.
ENTRYPOINT ["./hypercache_server"]