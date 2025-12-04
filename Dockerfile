# ===========================================================
# 1. Build stage
# ===========================================================
FROM debian:stable-slim AS builder

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    cmake \
    git \
    curl \
    wget \
    build-essential \
    libasio-dev \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source
COPY openflights_web_service.cpp .
COPY crow_all.h .

# Copy data files
COPY airlines.dat .
COPY airports.dat .
COPY routes.dat .

# Compile your Crow app
RUN g++ -std=c++17 openflights_web_service.cpp -o server -pthread -O3


# ===========================================================
# 2. Runtime stage
# ===========================================================
FROM debian:stable-slim

WORKDIR /app

# Copy compiled binary & data
COPY --from=builder /app/server .
COPY --from=builder /app/airlines.dat .
COPY --from=builder /app/airports.dat .
COPY --from=builder /app/routes.dat .

EXPOSE 8080
ENV PORT=8080

CMD ["./server"]
