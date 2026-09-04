FROM node:22-alpine

WORKDIR /app
COPY package.json server.js ./
RUN mkdir -p /data && chown node:node /data

ENV NODE_ENV=production
USER node
EXPOSE 8080

CMD ["node", "server.js"]
