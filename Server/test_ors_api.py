"""
Test script for OpenRouteService API key validation
Run this to verify your ORS API key works before using it in the main app
"""

import requests
import os
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

def test_ors_api():
    """Test OpenRouteService API with a simple 2-location matrix request"""
    
    api_key = os.environ.get('ORS_API_KEY', '')
    
    print("=" * 70)
    print("🧪 OpenRouteService API Key Test")
    print("=" * 70)
    
    if not api_key or api_key == 'your_api_key_here':
        print("❌ No valid API key found in .env file")
        print("\nPlease add your API key to server/.env:")
        print("   ORS_API_KEY=your_actual_key_here")
        return False
    
    print(f"✓ API Key found (length: {len(api_key)} chars)")
    print(f"  First 30 chars: {api_key[:30]}...")
    
    # Test endpoint: Matrix API with 2 simple locations
    url = "https://api.openrouteservice.org/v2/matrix/driving-car"
    
    # Simple test: Bangalore to nearby location
    payload = {
        "locations": [
            [77.5946, 12.9716],  # Bangalore city center [lng, lat]
            [77.7095, 12.9450]   # Whitefield, Bangalore [lng, lat]
        ],
        "metrics": ["distance"]
    }
    
    headers = {
        'Authorization': api_key,
        'Content-Type': 'application/json; charset=utf-8',
        'Accept': 'application/json'
    }
    
    print("\n🔄 Testing Matrix API endpoint...")
    print(f"   URL: {url}")
    print(f"   Locations: 2 test points")
    
    try:
        response = requests.post(url, json=payload, headers=headers, timeout=10)
        
        print(f"\n📡 Response Status: {response.status_code}")
        
        if response.status_code == 200:
            data = response.json()
            distances = data.get('distances', [])
            
            print("\n✅ SUCCESS! Your API key works!")
            print(f"   Matrix size: {len(distances)}x{len(distances[0])}")
            print(f"   Sample distance: {distances[0][1]:.0f} meters")
            print("\n👍 You can now use 'Actual distances (maps)' in your app")
            return True
            
        elif response.status_code == 403:
            error_data = response.json() if 'json' in response.headers.get('content-type', '') else {}
            error_msg = error_data.get('error', {})
            
            print("\n❌ 403 FORBIDDEN - Access Denied")
            print(f"   Error: {error_msg}")
            print("\n🔍 MOST LIKELY CAUSES:")
            print("   1. ⚠️  EMAIL NOT VERIFIED")
            print("      → Check your email inbox for OpenRouteService verification")
            print("      → Click the verification link in the email")
            print("      → Try again after verifying")
            print("\n   2. ⚠️  MATRIX API NOT ENABLED FOR YOUR KEY")
            print("      → Go to https://openrouteservice.org/dev/#/home")
            print("      → Dashboard → API Keys → Delete old key")
            print("      → Create new token → Enable 'Matrix' endpoint")
            print("\n   3. ⚠️  OLD/EXPIRED API KEY")
            print("      → Generate a fresh API key")
            print("      → Update .env file with new key")
            return False
            
        elif response.status_code == 401:
            print("\n❌ 401 UNAUTHORIZED - Invalid API key")
            print("   Your API key format is incorrect or invalid")
            print("   Generate a new one at: https://openrouteservice.org/dev/#/home")
            return False
            
        elif response.status_code == 429:
            print("\n⚠️  429 RATE LIMIT EXCEEDED")
            print("   Too many requests. Wait a few minutes and try again.")
            return False
            
        else:
            print(f"\n❌ Unexpected error: {response.status_code}")
            print(f"   Response: {response.text[:300]}")
            return False
            
    except requests.exceptions.Timeout:
        print("\n❌ Request timed out")
        print("   Check your internet connection")
        return False
        
    except Exception as e:
        print(f"\n❌ Error: {str(e)}")
        return False


if __name__ == "__main__":
    test_ors_api()
    print("\n" + "=" * 70)
