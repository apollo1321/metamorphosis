It is planned to prototype a queue on top of the Raft variant, in which, by
default, data is replicated into two replicas: in the location where the data
appears and in the location where it is consumed.

An additional replica, in a different availability zone, is used only if the
network between the source and the drain is broken. In the event of a network
outage or a breakdown of one of the replicas, data will begin to be copied to
another replica starting from the end. Thus, most of the time, data will be
sent between availability zones only 1 time.
