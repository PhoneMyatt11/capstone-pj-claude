# ===========================================================
# 1. Build stage (compile C++ Crow application)
# ===========================================================
FROM debian:stable-slim AS builder

RUN apt-get update && apt-get install -y \
    g++ cmake make git curl wget build-essential \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY openflights_web_service.cpp .
COPY crow_all.h .

COPY airlines.dat .
COPY airports.dat .
COPY routes.dat .

RUN g++ -std=c++17 openflights_web_service.cpp -o server -pthread -O3


# ===========================================================
# 2. Runtime stage
# ===========================================================
FROM debian:stable-slim

WORKDIR /app

COPY --from=builder /app/server .
COPY --from=builder /app/airlines.dat .
COPY --from=builder /app/airports.dat .
COPY --from=builder /app/routes.dat .

EXPOSE 8080

ENV PORT=8080

CMD ["./server"]
