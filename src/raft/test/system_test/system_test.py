#!/usr/bin/python3

import docker


if __name__ == "__main__":
    client = docker.from_env()
    client.containers.run("ubuntu", "echo hello world")
