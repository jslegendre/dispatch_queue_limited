# dispatch_queue_limited
A thread-limited dispatch queue with no locks or semaphores

## Summary
dispatch_queue_limited functions by wrapping a concurrent dispatch_queue_t in a convenient and thread-safe management layer.  With an efficient implementation relying solely on atomic operations, dispatch_queue_limited ensures optimal thread-safety without the need for locks or semaphores. This library is used internally by IconChamp to ensuring the UI accurately reflects the progress of many asynchronous XPC calls

## API
```
/*!
 * @function dispatch_queue_limited_create
 *
 * @abstract Creates a new limited dispatch queue with the specified limit and quality of service (QoS) class.
 *
 * @param limit      The maximum number of operations allowed to be executed concurrently on the queue.
 * @param qos           The quality of service class for the queue.
 * @return        A new dispatch queue with the specified limit and QoS class.
 */

DISPATCH_MALLOC DISPATCH_RETURNS_RETAINED DISPATCH_WARN_RESULT
dispatch_queue_limited_t
dispatch_queue_limited_create(uint32_t limit, dispatch_qos_class_t qos);


/*!
 * @function dispatch_queue_limited_set_target_queue
 *
 * @abstract Sets the target dispatch queue for the specified limited dispatch queue.
 *
 * @param dq     The limited dispatch queue.
 * @param queue  The target dispatch queue to which operations from the limited queue should be forwarded.
 */
void
dispatch_queue_limited_set_target_queue(dispatch_queue_limited_t dq, dispatch_queue_t queue);


/*!
 * @function dispatch_queue_limited_enqueue
 *
 * @abstract Enqueues a block to be executed asynchronously on the specified limited dispatch queue.
 *
 * @param dql  The limited dispatch queue.
 * @param op   The block to be executed.
 */
void
dispatch_queue_limited_enqueue(dispatch_queue_limited_t dql, void (^op)(void));


/*!
 * @function dispatch_queue_limited_enqueue_f
 *
 * @abstract Enqueues a function to be executed asynchronously on the specified limited dispatch queue, with an optional context.
 *
 * @param dq       The limited dispatch queue.
 * @param context  An optional context pointer to be passed to the function.
 * @param work     The function to be executed.
 */
void
dispatch_queue_limited_enqueue_f(dispatch_queue_limited_t dq, void *_Nullable context, dispatch_function_t work);


/*!
 * @function dispatch_queue_limited_enqueue_await
 *
 * @abstract Enqueues a block to be executed asynchronously on the specified limited dispatch queue but does not consider
 * the operation as complete until `dispatch_queue_limited_dequeue_await` is called.
 *
 * @discussion The 'await' variants are meant to be used when the queue needs  to maintain the operation count
 * when enqueuing asynchronous work.
 *
 * @param dql  The limited dispatch queue.
 * @param op   The block to be executed.
 */
void
dispatch_queue_limited_enqueue_await(dispatch_queue_limited_t dql, void (^op)(void));


/*!
 * @function dispatch_queue_limited_enqueue_await
 *
 * @abstract Enqueues a function to be executed asynchronously on the specified limited dispatch queue  but does not consider
 * the operation as complete until `dispatch_queue_limited_dequeue_await` is called.
 *
 * @discussion The ''await' variants are meant to be used to be used when you want to maintain your thread limit when enqueuing asynchronous work.
 *
 * @param dq       The limited dispatch queue.
 * @param context  An optional context pointer to be passed to the function.
 * @param work     The function to be executed.
 */
void
dispatch_queue_limited_enqueue_await_f(dispatch_queue_limited_t dq, void * _Nullable context, dispatch_function_t work);


/*!
 * @function dispatch_queue_limited_dequeue_await
 *
 * @abstract Tells the specified limited dispatch queue a previous enqueued 'await' operation is complete.
 *
 * @param dq  The limited dispatch queue.
 */
void
dispatch_queue_limited_dequeue_await(dispatch_queue_limited_t dq);
```

## Sample
```
  let dq = dispatch_queue_limited_create(4, QOS_CLASS_DEFAULT)
  let files:[String] = ["/file/path1", "/file/path2",...]
  
  for file in files {
      let fd = open(file, O_RDONLY)
      dispatch_queue_limited_enqueue_await(dq) {
          DispatchIO.read(fromFileDescriptor: fd, maxLength: 2048, runningHandlerOn: DispatchQueue.main, handler: { data, error in
              // Do something with data...
              
              dispatch_queue_limited_dequeue_await(dq)
          })
      }
  }

```
