### Implementation

The store is implemented as a multithreaded asynchronous server, in the sense that communications between client and server are asynchronous. The communications between vendors and server are synchronous. On receiving a list of queries from a client, the server contacts each vendor, in order, to reserve the product. If the server is not able to reserve any product fully, it will release all products reserved till then and returns a status of `false` to the client. If server was able to reserve all products, it will then try to sell (or rather buy on behalf of the client) all the products. If any error happens during sell or release operations, a status of `false` is returned to the client. However, the set of operations are not stopped, meaning that if there were 5 products to sell and if the 3rd sell call failed, we would still continue with the rest of the sell calls.


### Tests

-	Two clients - All quantities are within limits
-	Two clients - One client tries to buy excess and the other's quantity is within limits
-	Three clients - Total quantity brought by the clients exceeds the available.
-	Three clients - One client buys off everything in vendor 1
