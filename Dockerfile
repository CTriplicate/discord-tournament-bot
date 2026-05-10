# ── Build stage ────────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    g++-12 \
    libssl-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Set C++ compiler
ENV CXX=g++-12
ENV CC=gcc-12

WORKDIR /build

# Copy source
COPY . .

# Build the project
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# ── Runtime stage ─────────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN groupadd -r bot && useradd -r -g bot -m bot

WORKDIR /app

# Copy binary from builder
COPY --from=builder /build/build/discord_bot /app/discord_bot

# Copy default config (will be overridden by env vars or mounted volume)
COPY config.example.json /app/config.json

# Create data directory
RUN mkdir -p /app/data/brackets && chown -R bot:bot /app/data

USER bot

# Railway sets PORT but our bot doesn't use HTTP.
# We only need the BOT_TOKEN env var.
ENV BOT_TOKEN=""

ENTRYPOINT ["/app/discord_bot"]
CMD ["/app/config.json"]
