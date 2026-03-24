FROM node:20-alpine

WORKDIR /app

# Copy package files
COPY package*.json ./

# Install dependencies
RUN npm ci --only=production

# Copy application code
COPY server.js ./
COPY README.md ./

# Create data directory for persistence
RUN mkdir -p /app/data

# Expose port
EXPOSE 3000

# Run as non-root user
USER node

# Start the server
CMD ["node", "server.js"]
