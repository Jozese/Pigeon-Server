import subprocess

def check_package(package) -> bool:
    try:
        subprocess.check_output(["dpkg-query","-l",package])
        return True
    except subprocess.CalledProcessError:
        return False
    

packages = ["build-essential","libjsoncpp-dev","libssl-dev"]
print("Querying packages")
print("*********************************************")
for k in packages:
    if check_package(k):
        print(f"{k} installed")
        
    else:
        print(f"{k} not installed. Run sudo apt install {k}")
        exit(1)
    print("*********************************************")

print("Compiling")
subprocess.call(f"./build.sh", shell=True)
