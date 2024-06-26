#!/usr/bin/python3

import argparse
import docker
import logging
import pathlib
import random
import time

DOCKER_IMAGE_TAG = "morf-raft-exe"
DOCKER_NETWORK_NAME = "morf-raft-net"
DOCKER_CONTAINER_LABEL = "morf-raft-test"

RAFT_NODE_PORT = 10050


def clear_test_environment(client: docker.DockerClient):
    for container in client.containers.list(all=True, filters={"label": DOCKER_CONTAINER_LABEL}):
        logging.info("Removing container {}".format(container))
        container.remove(force=True)

    for network in client.networks.list(names=[DOCKER_NETWORK_NAME]):
        logging.info("Removing network {}".format(network))
        network.remove()


def build_docker_image(client: docker.DockerClient, docker_dir: str):
    for log in client.api.build(path=docker_dir, tag=DOCKER_IMAGE_TAG, decode=True):
        for key, value in log.items():
            if type(value) is not str:
                continue
            value = value.strip()
            if not value:
                continue
            if key == "error":
                logging.error(value.strip())
                raise RuntimeError("cannot build docker image")
            else:
                logging.info(value.strip())


def make_node_list(node_count) -> str:
    node_list = []
    for i in range(node_count):
        node_list.append("raft_node_{}:{}".format(i, RAFT_NODE_PORT))
    return " ".join(node_list)


def make_raft_node_run_cmd(node_count, node_id) -> str:
    return ("./mtf_raft_test_node "
            "--raft-nodes {} "
            "--node-id {} "
            "--store-path {}"
            .format(make_node_list(node_count),
                    node_id,
                    "/app"))


def make_raft_client_run_cmd(node_count) -> str:
    return "./mtf_raft_test_client --raft-nodes {} random".format(make_node_list(node_count))


def run_raft_nodes_cluster(client: docker.DockerClient, node_count) -> list:
    nodes = []
    for node_id in range(node_count):
        nodes.append(client.containers.run(
            DOCKER_IMAGE_TAG,
            make_raft_node_run_cmd(node_count, node_id),
            name="raft_node_{}".format(node_id),
            detach=True,
            network=DOCKER_NETWORK_NAME,
            labels=[DOCKER_CONTAINER_LABEL],
        ))
    return nodes


def run_raft_client_cluster(client: docker.DockerClient, client_count, node_count) -> list:
    nodes = []
    for node_id in range(client_count):
        nodes.append(client.containers.run(
            DOCKER_IMAGE_TAG,
            make_raft_client_run_cmd(node_count),
            name="raft_client_{}".format(node_id),
            detach=True,
            labels=[DOCKER_CONTAINER_LABEL],
        ))
    return nodes


def write_containers_logs(nodes, iteration, logs_dir, test_name):
    if logs_dir is None:
        return
    logging.info("Writing logs to {}".format(logs_dir))
    for node in nodes:
        text = node.logs().decode()
        with open(logs_dir + "/{}_{}.{}.txt".format(test_name, iteration, node.name), "w") as f:
            f.write(text)


def check_history(client: docker.DockerClient, raft_clients, minimum_successes):
    history = b""
    for node in raft_clients:
        history += node.logs()

    container = client.api.create_container(
        DOCKER_IMAGE_TAG,
        "./mtf_history_checker_exe",
        name="history_checker",
        labels=[DOCKER_CONTAINER_LABEL],
        stdin_open=True,
    )

    sock = client.api.attach_socket(
        container, params={"stdin": 1, "stdout": 1, "stderr": 1, "stream": 1})
    client.api.start(container)

    sock._sock.send(history)
    sock._sock.close()
    sock.close()

    status = client.api.wait(container)
    retcode = status['StatusCode']
    output = client.api.logs(container)

    successes = int(output.splitlines()[0].split()[1])
    logging.info("Successes: {}".format(successes))

    second_line = output.splitlines()[1]
    if second_line.startswith(b"Fail"):
        raise RuntimeError("Linearizability check failed: {}".format(second_line))

    if successes < minimum_successes:
        raise RuntimeError("Too few requests have succeeded")

    if retcode != 0:
        raise RuntimeError("Non-zero retcode")


def run_simple_test(client: docker.DockerClient,
                    iteration,
                    logs_dir,
                    node_count,
                    client_count):
    clear_test_environment(client)

    test_name = "simple_r_{}_c_{}".format(node_count, client_count)

    logging.info("START TEST: {}".format(test_name))

    logging.info("Creating network {}".format(DOCKER_NETWORK_NAME))
    network = client.networks.create(DOCKER_NETWORK_NAME, driver="ipvlan")

    logging.info("Starting raft nodes")
    raft_nodes = run_raft_nodes_cluster(client, node_count=node_count)

    logging.info("Wait for 1s")
    time.sleep(1)

    logging.info("Starting raft clients")
    raft_clients = run_raft_client_cluster(client, client_count=client_count, node_count=node_count)

    logging.info("Connect raft clients to network")
    for container in raft_clients:
        network.connect(container)

    logging.info("Wait for 5s")
    time.sleep(5)

    logging.info("Pause raft client")
    for node in raft_clients:
        node.pause()

    logging.info("Killing containers")
    for node in raft_nodes + raft_clients:
        node.kill()

    write_containers_logs(raft_nodes + raft_clients, iteration,
                          logs_dir, test_name)

    logging.info("Check client history")
    check_history(client, raft_clients, 50)


def run_crash_test(client: docker.DockerClient,
                   iteration,
                   logs_dir,
                   node_count,
                   client_count):
    clear_test_environment(client)

    test_name = "crash_r_{}_c_{}".format(node_count, client_count)

    logging.info("START TEST: {}".format(test_name))

    logging.info("Creating network {}".format(DOCKER_NETWORK_NAME))
    network = client.networks.create(DOCKER_NETWORK_NAME, driver="ipvlan")

    logging.info("Starting raft nodes")
    raft_nodes = run_raft_nodes_cluster(client, node_count=node_count)

    logging.info("Starting raft clients")
    raft_clients = run_raft_client_cluster(client, client_count=client_count, node_count=node_count)

    logging.info("Connect raft clients to network")
    for container in raft_clients:
        network.connect(container)

    actions = ["disconnect", "connect",
               "stop", "start",
               "pause", "unpause"]
    weights = [1, 4,
               1, 4,
               1, 4]

    start_time = time.time()
    while time.time() - start_time < 20:
        sleep_time = 1 * random.random()
        logging.info("Sleep for {}s".format(sleep_time))
        time.sleep(sleep_time)

        action = random.choices(actions, weights)[0]
        node = random.choices(raft_nodes)[0]
        logging.info("Start action {} for node {}".format(action, node.name))

        try:
            if action == "disconnect":
                network.disconnect(node)
            if action == "connect":
                network.connect(node)
            if action == "stop":
                node.stop(timeout=0)
            if action == "start":
                node.start()
            if action == "pause":
                node.pause()
            if action == "unpause":
                node.unpause()
        except docker.errors.DockerException as e:
            logging.info("Could not make action: {}".format(e))

    logging.info("Wait for 2s")
    time.sleep(2)

    logging.info("Killing containers")
    for node in raft_nodes + raft_clients:
        try:
            node.kill()
        except docker.errors.DockerException:
            pass

    write_containers_logs(raft_nodes + raft_clients, iteration,
                          logs_dir, test_name)

    logging.info("Check client history")
    check_history(client, raft_clients, 20)


def main():
    logging.basicConfig(format="%(asctime)s %(levelname)s %(message)s",
                        datefmt="[%H:%M:%S]", level=logging.INFO)

    parser = argparse.ArgumentParser(description="Raft system tests")

    parser.add_argument("--logs-dir", help="path to directory to store logs")
    parser.add_argument("--iterations", default=1, help="test iterations count", type=int)

    args = parser.parse_args()

    logging.info("Starting client")
    client = docker.from_env()
    client.close()

    clear_test_environment(client)

    root_dir = pathlib.Path(__file__ + "/../../../../../").resolve()

    logging.info("Building image {}".format(DOCKER_IMAGE_TAG))
    build_docker_image(client, str(root_dir))

    try:
        for iteration in range(args.iterations):
            run_simple_test(client, iteration, args.logs_dir,
                            node_count=3, client_count=2)
            run_simple_test(client, iteration, args.logs_dir,
                            node_count=2, client_count=3)
            run_simple_test(client, iteration, args.logs_dir,
                            node_count=5, client_count=6)

            run_crash_test(client, iteration, args.logs_dir,
                           node_count=3, client_count=2)
            run_crash_test(client, iteration, args.logs_dir,
                           node_count=3, client_count=5)
    finally:
        clear_test_environment(client)


if __name__ == "__main__":
    main()
