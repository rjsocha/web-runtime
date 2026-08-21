# swarm

`stack.yaml` deploys anywhere, `stack-linux.yaml` and `stack-windows.yaml` are placed by the `OS` node label:

```yaml
      placement:
        constraints:
          - node.labels.OS == LINUX
```

Label the nodes first, once per node:

```sh
docker node update --label-add OS=LINUX linux1
docker node update --label-add OS=WINDOWS windows1
docker node update --label-rm OS linux2
```

What is set where:

```sh
docker service inspect $(docker service ls -q) --format '{{ .Spec.Name }}: {{ range $k,$v := .Spec.TaskTemplate.Placement.Constraints }}{{$k}}={{$v}} {{end}}'
docker node inspect $(docker node ls -q) --format '{{ .Description.Hostname }}: {{ range $k,$v := .Spec.Labels }}{{$k}}={{$v}} {{end}}'
```

Both run on a manager.
