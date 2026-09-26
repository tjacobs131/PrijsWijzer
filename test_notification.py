from http.server import BaseHTTPRequestHandler, HTTPServer
import json

class NotificationHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        
        # The JSON payload you want Android to download
        payload = {
            "title": "Price Drop!",
            "message": "Item X is now cheaper.",
            "id": 105
        }
        self.wfile.write(json.dumps(payload).encode('utf-8'))
        print("Sent JSON payload to Android device.")

server = HTTPServer(('0.0.0.0', 8080), NotificationHandler)
print("Local JSON server running on port 8080...")
server.serve_forever()