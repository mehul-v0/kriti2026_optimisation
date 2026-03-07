FROM python:3.11-slim

# Install build tools for C++ compiler
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy C++ source files and compile
# (Makefile uses -march=native which compiles for THIS server's CPU = optimal performance)
COPY src/ ./src/
COPY Makefile ./
RUN make clean 2>/dev/null || true && make

# Install Python dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy Python application files
COPY *.py ./

# Copy solver config
COPY solver_config.json ./

# Create required directories
RUN mkdir -p output uploads

# Expose port
EXPOSE 5000

# Single worker (solver is CPU-heavy, multiple workers = memory contention)
# Timeout 300s = 5 min (large VRP problems can take 2-3 min)
CMD ["gunicorn", "--bind", "0.0.0.0:5000", "--workers", "1", "--timeout", "300", "--keep-alive", "5", "app:app"]
