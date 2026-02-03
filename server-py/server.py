from flask import Flask, jsonify, request
import subprocess
import json
from urllib.parse import quote
from datetime import datetime
import config

class Task:
    def __init__(self, id, title, completed, due):
        self.id = id
        self.title = title
        self.completed = completed
        self.due = due

app = Flask(__name__)

@app.route('/run-command', methods=['GET'])
def run_external_command():
    prog = "reminders"
    option = "show-list"
    arg1 = "Reminders"
    command = [prog, option, arg1, "-f", "json"] # reminders command and return as json
    
    try:
        # Execute the command
        result = subprocess.run(
            command,
            capture_output=True, # Capture stdout and stderr
            text=True,           # Decode output as string (Python 3.7+)
            check=True           # Raise exception if the command fails
        )

#        return jsonify({
#            "status": "success",
#            "stdout": result.stdout,
#            "stderr": result.stderr
#        })
        return json.loads(result.stdout)

    except subprocess.CalledProcessError as e:
        # Handle errors if the command returns a non-zero exit code
        return jsonify({
            "status": "error",
            "message": f"Command failed with return code {e.returncode}",
            "stdout": e.stdout,
            "stderr": e.stderr
        }), 500
    except FileNotFoundError as e:
        # Handle cases where the executable is not found
        return jsonify({
            "status": "error",
            "message": f"Executable not found: {e.filename}"
        }), 404
    except Exception as e:
        # Handle other potential exceptions
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@app.route("/tasks", methods=["GET"])
def get_tasks():
    cmd = "show-all"
    command = ["reminders", cmd, "-f", "json"] # reminders command and return as json
    
    try:
        # Execute the command
        result = subprocess.run(
            command,
            capture_output=True, # Capture stdout and stderr
            text=True,           # Decode output as string (Python 3.7+)
            check=True           # Raise exception if the command fails
        )

        # add an "id" field with the index number that is part of the command line
        #   result of the reminders command line program
        data = json.loads(result.stdout)
        new_data = [
            {"id": i, **row}
            for i, row in enumerate(data, start=0)
        ]

        return new_data

    except subprocess.CalledProcessError as e:
        # Handle errors if the command returns a non-zero exit code
        return jsonify({
            "status": "error",
            "message": f"Command failed with return code {e.returncode}",
            "stdout": e.stdout,
            "stderr": e.stderr
        }), 500
    except FileNotFoundError as e:
        # Handle cases where the executable is not found
        return jsonify({
            "status": "error",
            "message": f"Executable not found: {e.filename}"
        }), 404
    except Exception as e:
        # Handle other potential exceptions
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@app.route("/tasks/lists", methods=["GET"])
def get_lists():
    cmd = "show-lists"

    command = ["reminders", cmd, "-f", "json"] # reminders command and return as json
    
    try:
        # Execute the command
        result = subprocess.run(
            command,
            capture_output=True, # Capture stdout and stderr
            text=True,           # Decode output as string (Python 3.7+)
            check=True           # Raise exception if the command fails
        )

        return json.loads(result.stdout)

    except subprocess.CalledProcessError as e:
        # Handle errors if the command returns a non-zero exit code
        return jsonify({
            "status": "error",
            "message": f"Command failed with return code {e.returncode}",
            "stdout": e.stdout,
            "stderr": e.stderr
        }), 500
    except FileNotFoundError as e:
        # Handle cases where the executable is not found
        return jsonify({
            "status": "error",
            "message": f"Executable not found: {e.filename}"
        }), 404
    except Exception as e:
        # Handle other potential exceptions
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500
             
@app.route("/tasks/lists/<listname>", methods=["GET"])
def get_task_by_list(listname):
    cmd = "show"
    encoded_string = listname
 
    command = ["reminders", cmd, encoded_string, "-f", "json"] # reminders command and return as json
    
    try:
        # Execute the command
        result = subprocess.run(
            command,
            capture_output=True, # Capture stdout and stderr
            text=True,           # Decode output as string (Python 3.7+)
            check=True           # Raise exception if the command fails
        )

        # add an "id" field with the index number that is part of the command line
        #   result of the reminders command line program
        data = json.loads(result.stdout)
        new_data = [
            {"id": i, **row}
            for i, row in enumerate(data, start=0)
        ]

        return new_data

    except subprocess.CalledProcessError as e:
        # Handle errors if the command returns a non-zero exit code
        return jsonify({
            "status": "error",
            "message": f"Command failed with return code {e.returncode}",
            "stdout": e.stdout,
            "stderr": e.stderr
        }), 500
    except FileNotFoundError as e:
        # Handle cases where the executable is not found
        return jsonify({
            "status": "error",
            "message": f"Executable not found: {e.filename}"
        }), 404
    except Exception as e:
        # Handle other potential exceptions
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

# @app.route("/tasks/complete", methods=["POST"])
# def complete():
#    data = request.json
#    success = complete_task(data["id"])
#    return jsonify({"success": success}

# To test the /tasks/complete route
# curl -X POST http://127.0.0.1:8080/tasks/complete \
#     -H "Content-Type: application/json" \
#     -d '{"task_id": 123, "list": "<list name>"}'

@app.route("/tasks/complete", methods=["POST"])
def complete():
    data = request.json
    # success = complete_task(data["id"])
    # return jsonify({"command": "complete"})
    # Get JSON payload
    data = request.get_json()

    # Print JSON to console for testing
    #print("Received JSON:", data)

    # Simple response
#    return jsonify({
#        "status": "success",
#        "received": data
#    }), 200
    cmd = "complete"
    task_id = data.get("task_id") if data else None
    listname = data.get("list") if data else None
    encoded_string = listname
 
    command = ["reminders", cmd, encoded_string, task_id] # reminders command and return as json
    
    try:
        # Execute the command
        result = subprocess.run(
            command,
            capture_output=True, # Capture stdout and stderr
            text=True,           # Decode output as string (Python 3.7+)
            check=True           # Raise exception if the command fails
        )

        #return json.loads(result.stdout)
        return result.stdout

    except subprocess.CalledProcessError as e:
        # Handle errors if the command returns a non-zero exit code
        return jsonify({
            "status": "error",
            "message": f"Command failed with return code {e.returncode}",
            "stdout": e.stdout,
            "stderr": e.stderr
        }), 500
    except FileNotFoundError as e:
        # Handle cases where the executable is not found
        return jsonify({
            "status": "error",
            "message": f"Executable not found: {e.filename}"
        }), 404
    except Exception as e:
        # Handle other potential exceptions
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

# To test the /tasks/uncomplete route
# curl -X POST http://127.0.0.1:8080/tasks/uncomplete \
#     -H "Content-Type: application/json" \
#     -d '{"task_id": 123, "list": "<list name>"}'

@app.route("/tasks/uncomplete", methods=["POST"])
def uncomplete():
    data = request.json
    # success = complete_task(data["id"])
    # return jsonify({"command": "uncomplete"})
    # Get JSON payload
    data = request.get_json()

    # Print JSON to console for testing
    #print("Received JSON:", data)

    # Simple response
#    return jsonify({
#        "status": "success",
#        "received": data
#    }), 200
    cmd = "uncomplete"
    task_id = data.get("task_id") if data else None
    listname = data.get("list") if data else None
    encoded_string = listname
 
    command = ["reminders", cmd, encoded_string, task_id] # reminders command and return as json
    
    try:
        # Execute the command
        result = subprocess.run(
            command,
            capture_output=True, # Capture stdout and stderr
            text=True,           # Decode output as string (Python 3.7+)
            check=True           # Raise exception if the command fails
        )

        return json.loads(result.stdout)

    except subprocess.CalledProcessError as e:
        # Handle errors if the command returns a non-zero exit code
        return jsonify({
            "status": "error",
            "message": f"Command failed with return code {e.returncode}",
            "stdout": e.stdout,
            "stderr": e.stderr
        }), 500
    except FileNotFoundError as e:
        # Handle cases where the executable is not found
        return jsonify({
            "status": "error",
            "message": f"Executable not found: {e.filename}"
        }), 404
    except Exception as e:
        # Handle other potential exceptions
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@app.route("/tasks/update", methods=["POST"])
def update():
    data = request.json
    # success = complete_task(data["id"])
    return jsonify({"command": "update"})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=config.hostInfo['host_port'])
    

#curl -X POST http://127.0.0.1:8080/tasks/complete \
#     -H "Content-Type: application/json" \
#     -d '{"task_id": "4D20F1AE-F2B3-4BA1-94C1-8FB7B6500523", "list": "Shopping List"}'