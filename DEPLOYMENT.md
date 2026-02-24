# Deployment Guide - Kriti 2026 VRP System

## Architecture Overview

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Frontend  │    │   Backend   │    │ Flutter App │
│   (React)   │───▶│   (Flask)   │◀───│   (Mobile)  │
│   Vercel    │    │  Container  │    │  App Store  │
└─────────────┘    └─────────────┘    └─────────────┘
```

## Option 1: Railway (Easiest - Recommended for Hackathon)

### Backend Deployment

1. **Create Dockerfile** in `/server`:
```dockerfile
FROM python:3.11-slim

# Install build tools for C++ compiler
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy and compile C++ solver
COPY *.cpp *.h Makefile ./
RUN make clean && make

# Install Python dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy Python app
COPY *.py ./
COPY output/ ./output/

EXPOSE 5000

CMD ["python", "app.py"]
```

2. **Deploy to Railway**:
   - Go to [railway.app](https://railway.app)
   - Connect GitHub repo
   - Set root directory to `server/`
   - Railway auto-detects Dockerfile
   - Get your API URL: `https://your-app.railway.app`

### Frontend Deployment (Vercel)

1. Go to [vercel.com](https://vercel.com)
2. Import GitHub repo
3. Set root directory: `frontend/`
4. Set environment variable:
   ```
   VITE_API_URL=https://your-app.railway.app
   ```
5. Deploy

**Total cost**: Free tier available

---

## Option 2: Render (Good Free Tier)

### Backend
1. Create `render.yaml` in repo root:
```yaml
services:
  - type: web
    name: vrp-solver
    env: docker
    dockerfilePath: server/Dockerfile
    plan: free
```

2. Connect repo on [render.com](https://render.com)

### Frontend
- Same as Vercel, or use Render Static Site

---

## Option 3: AWS (Production-Grade)

### Architecture
```
CloudFront ──▶ S3 (Frontend)
     │
     ▼
API Gateway ──▶ ECS/Fargate (Docker) ──▶ RDS (optional)
```

### Steps
1. **Frontend**: `npm run build` → Upload to S3 → CloudFront CDN
2. **Backend**: Push Docker to ECR → Deploy on ECS Fargate
3. **API**: Set up API Gateway for HTTPS endpoint

**Cost**: ~$20-50/month depending on traffic

---

## Option 4: DigitalOcean App Platform

Simple PaaS similar to Railway:
1. Create App from GitHub
2. Set Dockerfile path for backend
3. Add static site for frontend

**Cost**: $5/month for basic droplet

---

## Quick Local Testing Before Deploy

```bash
# Build frontend
cd frontend
npm run build

# Test production build
npm run preview

# Backend - compile solver
cd ../server
make clean && make

# Run Flask
python app.py
```

---

## Environment Variables

### Frontend (.env.production)
```env
VITE_API_URL=https://your-backend-url.com
```

### Backend (.env)
```env
FLASK_ENV=production
PORT=5000
```

---

## Flutter App Deployment

### Android (Google Play)
```bash
cd App/flutter_application_1
flutter build apk --release
# Upload to Play Console
```

### iOS (App Store)
```bash
flutter build ios --release
# Upload via Xcode to App Store Connect
```

---

## Recommended for Your Use Case

| Scenario | Recommendation |
|----------|---------------|
| **Hackathon Demo** | Railway (free, fast setup) |
| **Production MVP** | Render or Railway Pro |
| **Enterprise** | AWS ECS + CloudFront |
| **Budget** | DigitalOcean $5 Droplet |

## My Recommendation: Railway + Vercel

**Why:**
1. ✅ Free tier sufficient for demos
2. ✅ Handles Docker (needed for C++ solver)
3. ✅ Auto-deploys from GitHub
4. ✅ HTTPS included
5. ✅ Easy environment variables
6. ✅ Can scale later if needed

**Setup time**: ~30 minutes
