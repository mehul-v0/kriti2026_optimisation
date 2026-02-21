# OpenRouteService Setup Guide

## What is OpenRouteService?

OpenRouteService (ORS) is a free, open-source routing service based on OpenStreetMap data. It provides **actual road distances** instead of straight-line (haversine) calculations.

## Free Tier Limits

- **2,000 requests per day**
- **40 requests per minute**
- **Up to 50 locations** per matrix request

This is plenty for most use cases!

## Setup Instructions

### Option 1: Get a Free API Key (Recommended)

1. **Sign up** at https://openrouteservice.org/dev/#/signup
2. **Verify your email**
3. **Create a new token** in your dashboard
4. **Copy your API key** (looks like: `5b3ce3597851110001cf6248...`)

### Option 2: Use Without API Key (Limited)

The system will work without an API key, but with stricter rate limits. If ORS fails, it automatically falls back to haversine with a 1.3x multiplier.

## How to Use Your API Key

### On Windows:

**Temporary (current session only):**
```cmd
set ORS_API_KEY=your_api_key_here
python app.py
```

**Permanent (for your user account):**
```cmd
setx ORS_API_KEY "your_api_key_here"
```
Then restart your terminal and run:
```cmd
python app.py
```

### On Linux/Mac:

**Temporary:**
```bash
export ORS_API_KEY=your_api_key_here
python app.py
```

**Permanent:**
Add to your `~/.bashrc` or `~/.zshrc`:
```bash
export ORS_API_KEY=your_api_key_here
```
Then:
```bash
source ~/.bashrc
python app.py
```

## How It Works

1. **Frontend**: Select "Actual Map Routes" in the optimization settings
2. **Backend**: Detects the setting and calls OpenRouteService Matrix API
3. **Distance Calculation**: Fetches real road distances for all locations
4. **Solver**: Uses actual distances instead of approximations
5. **Fallback**: If ORS fails, automatically uses haversine with 1.3x multiplier

## Testing

To test if it's working:

1. Upload your Excel file with employee and vehicle data
2. In Data Insights page, select **"Actual Map Routes"** as distance method
3. Click "Run Optimization"
4. Check the backend console - you should see:

```
=== Fetching Actual Road Distances from OpenRouteService ===
Fetching distances for 61 locations...
Using OpenRouteService with API key
✓ Successfully fetched actual road distances from OpenRouteService
```

## Troubleshooting

### "No ORS_API_KEY found"
- The system will try the public endpoint (may be rate-limited)
- Set up your API key using instructions above

### "OpenRouteService API error: 401"
- Your API key is invalid or expired
- Get a new key from https://openrouteservice.org/dev/#/

### "OpenRouteService API error: 429"
- You've hit the rate limit (40 requests/minute or 2000/day)
- Wait a few minutes or upgrade your ORS plan

### "Locations exceed ORS limit (50)"
- For large datasets (>50 locations), the system automatically falls back to haversine
- Consider using a different routing service or batching requests

### Request timed out
- Network issue or ORS is slow
- The system automatically falls back to haversine

## Comparing Results

**Haversine (straight-line)**:
- Faster computation
- Underestimates actual travel distance
- Good for initial estimates

**Haversine with 1.3x multiplier**:
- Quick approximation of road distances
- ~30% longer than straight-line
- Reasonable for urban areas

**OpenRouteService (actual roads)**:
- Most accurate
- Considers actual road network
- Best for production use

## Cost Comparison

| Service | Free Tier | Cost After Free |
|---------|-----------|-----------------|
| OpenRouteService | 2,000 req/day | Contact for enterprise |
| Google Maps | $200 credit/month | $5 per 1000 requests |
| Mapbox | 100,000 req/month | $0.60 per 1000 requests |

**Recommendation**: Start with OpenRouteService free tier - it's more than enough for most use cases!

## Example API Response

For 3 locations, ORS returns distances in meters:
```json
{
  "distances": [
    [0, 5234.2, 8921.5],
    [5234.2, 0, 3687.3],
    [8921.5, 3687.3, 0]
  ]
}
```

The system automatically converts this to kilometers and builds the distance matrix for the solver.
