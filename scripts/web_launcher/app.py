import os
import subprocess
from flask import Flask, jsonify, render_template, request

app = Flask(__name__)

# The systemd service name controlling the ROS2 launch
SERVICE_NAME = "jetcar.service"

def run_systemctl(action):
    """Executes a systemctl command on the jetcar service."""
    try:
        # Using sudo because systemctl start/stop requires privileges.
        # Sudoers should be configured to allow this without a password.
        result = subprocess.run(
            ["sudo", "systemctl", action, SERVICE_NAME],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        if result.returncode == 0:
            return True, f"Successfully executed '{action}'"
        else:
            return False, result.stderr.strip()
    except Exception as e:
        return False, str(e)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/status", methods=["GET"])
def get_status():
    """Checks if the systemd service is currently active."""
    try:
        result = subprocess.run(
            ["systemctl", "is-active", SERVICE_NAME],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        status = result.stdout.strip()
        is_running = (status == "active")
        return jsonify({
            "status": status,
            "running": is_running
        })
    except Exception as e:
        return jsonify({"status": "unknown", "running": False, "error": str(e)}), 500

@app.route("/api/start", methods=["POST"])
def start_robot():
    """Starts the jetcar systemd service."""
    success, message = run_systemctl("start")
    return jsonify({
        "success": success,
        "message": message
    }), 200 if success else 500

@app.route("/api/stop", methods=["POST"])
def stop_robot():
    """Stops the jetcar systemd service."""
    success, message = run_systemctl("stop")
    return jsonify({
        "success": success,
        "message": message
    }), 200 if success else 500

@app.route("/api/logs", methods=["GET"])
def get_logs():
    """Fetches the last 50 lines of logs for the jetcar service."""
    try:
        result = subprocess.run(
            ["journalctl", "-u", SERVICE_NAME, "-n", "50", "--no-pager"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return jsonify({
            "logs": result.stdout
        })
    except Exception as e:
        return jsonify({"logs": f"Error fetching logs: {str(e)}"}), 500

if __name__ == "__main__":
    # Host on all interfaces, port 8080
    app.run(host="0.0.0.0", port=8080, debug=True)
