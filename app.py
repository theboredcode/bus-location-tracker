from flask import Flask, request, render_template, make_response
from threading import Lock

app = Flask(__name__)
location = {"lat": 0.0, "lon": 0.0}
location_lock = Lock()

@app.route('/')
def live_location():
    with location_lock:
        response = make_response(render_template("index.html", lat=location["lat"], lon=location["lon"]))
        response.headers['Cache-Control'] = 'no-store, no-cache, must-revalidate, max-age=0'
        return response

@app.route('/update', methods=['POST'])
def update_location():
    try:
        print("Received POST request with form data:", request.form)
        lat = request.form.get("lat")
        lon = request.form.get("lon")
        
        if not lat or not lon:
            print("Error: Missing latitude or longitude")
            return "Missing latitude or longitude", 400
            
        try:
            lat = float(lat)
            lon = float(lon)
            
            if not (-90 <= lat <= 90) or not (-180 <= lon <= 180):
                print(f"Error: Invalid coordinates - lat: {lat}, lon: {lon}")
                return "Invalid coordinate values", 400
                
        except ValueError as ve:
            print(f"Error: Invalid number format - lat: {lat}, lon: {lon}, error: {ve}")
            return "Invalid number format", 400
            
        with location_lock:
            location["lat"] = lat
            location["lon"] = lon
            print(f"Location updated to lat: {lat}, lon: {lon}")
            
        return "Location updated", 200
        
    except Exception as e:
        print(f"Error updating location: {str(e)}")
        return f"Error updating location: {str(e)}", 500

if __name__ == '__main__':
    import os
    port = int(os.getenv("PORT", 5000))
    app.run(debug=False, host="0.0.0.0", port=port)
