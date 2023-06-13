//
//  dispatch_queue_limited.h
//  IconChamp
//
//  Created by Jeremy on 5/21/23.
//

#ifndef dispatch_queue_limited_h
#define dispatch_queue_limited_h

#import <dispatch/dispatch.h>

DISPATCH_ASSUME_NONNULL_BEGIN

OS_OBJECT_DECL_CLASS(dispatch_queue_limited);

__BEGIN_DECLS

/*!
 * @define DISPATCH_QUEUE_LIMITED_AWAIT_BLOCKING
 *
 * @abstract Enable this define if your workload relies heavily on using `dispatch_queue_limited_enqueue_await`
 * for multiple small asynchronous tasks. Enabling this define keeps the enqueue-await threads alive by blocking until
 * dequeue-await is called. This approach eliminates the overhead of thread destruction and creation, but it also means
 * holding a thread from the root queue.
 *
 * @discussion Use this define carefully and only if your workload requires it, as it may have implications on the overall
 * system resource usage and performance.
 */
#ifndef DISPATCH_QUEUE_LIMITED_AWAIT_BLOCKING
#define DISPATCH_QUEUE_LIMITED_AWAIT_BLOCKING 0
#endif

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


/*!
 * @function dispatch_queue_limited_retain
 *
 * @abstract Increment the reference count the specified limited dispatch queue
 *
 * @param dq The limited dispatch queue to retain
 */
DISPATCH_SWIFT_UNAVAILABLE("Can't be used with ARC")
void
dispatch_queue_limited_retain(dispatch_queue_limited_t dq);

/*!
 * @function dispatch_queue_limited_release
 *
 * @abstract Decrement the reference count the specified limited dispatch queue
 *
 * @param dq The limited dispatch queue to retain
 */
DISPATCH_SWIFT_UNAVAILABLE("Can't be used with ARC")
void
dispatch_queue_limited_release(dispatch_queue_limited_t dq);

__END_DECLS
DISPATCH_ASSUME_NONNULL_END
#endif /* dispatch_queue_limited_h */
