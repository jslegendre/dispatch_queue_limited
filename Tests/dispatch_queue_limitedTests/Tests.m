//
//  Tests.m
//  
//
//  Created by Jeremy on 8/26/23.
//

#import <XCTest/XCTest.h>
#import <dispatch_queue_limited.h>
#import <stdatomic.h>

/* 
 * ***************************************
 *  Compile and run with thread sanitizer
 * ***************************************
 */

typedef struct {
    int limit;
    volatile _Atomic int32_t count;
    volatile bool testPassed;
} c_func_test_ctxt;

void testFunction(void *context) {
    c_func_test_ctxt *ctxt = (c_func_test_ctxt *)context;
    
    
    int32_t c = atomic_fetch_add_explicit(&ctxt->count, 1, memory_order_relaxed);
    if (c >= ctxt->limit || !ctxt->testPassed) {
        ctxt->testPassed = false;
    }
    
    printf("func test: count = %d\n", c+1);
    usleep(5000);
    atomic_fetch_sub_explicit(&ctxt->count, 1, memory_order_relaxed);
}


@interface Tests : XCTestCase

@end

@implementation Tests

- (void)testFuncLimit {
    uint32_t limit = 4;
    dispatch_queue_limited_t dql = dispatch_queue_limited_create(limit, QOS_CLASS_DEFAULT);
    c_func_test_ctxt *ctxt = malloc(sizeof(c_func_test_ctxt));
    ctxt->limit = 4;
    ctxt->count = 0;
    ctxt->testPassed = true;
    
    for (int i = 0; i < 9; i++) {
        dispatch_queue_limited_enqueue_f(dql, ctxt, testFunction);
    }
    
    sleep(1);
    XCTAssertTrue(ctxt->testPassed);
    free(ctxt);
}

- (void)testBlockLimit {
    
    volatile __block bool testPassed = true;
    volatile __block _Atomic int32_t count = 0;
    
    uint32_t limit = 4;
    dispatch_queue_limited_t dql = dispatch_queue_limited_create(limit, QOS_CLASS_DEFAULT);
    
    for (int i = 1; i < 10; i++) {
        dispatch_queue_limited_enqueue(dql, ^{
            int32_t c = atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
            if (c >= limit || !testPassed) {
                testPassed = false;
            }
            
            printf("thread %d: count = %d\n", i, c+1);
            usleep(5000);
            atomic_fetch_sub_explicit(&count, 1, memory_order_relaxed);
            
        });
    }
    
    sleep(1);
    XCTAssertTrue(testPassed);
}

// Test to make sure the result of `testBlockLimit` is valid
- (void)testBlockLimit_Fail {
    
    volatile __block bool testPassed = true;
    volatile __block _Atomic int32_t count = 0;
    
    uint32_t limit = 4;
    dispatch_queue_limited_t dql = dispatch_queue_limited_create(limit, QOS_CLASS_DEFAULT);
    
    for (int i = 1; i < 10; i++) {
        dispatch_queue_limited_enqueue(dql, ^{
            int32_t c = atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
            if (c >= limit || !testPassed) {
                testPassed = false;
            }
            
            printf("thread %d: count = %d\n", i, c+1);
            usleep(5000);
            
        });
    }
    
    sleep(1);
    XCTAssertFalse(testPassed);
}

- (void)testSerialAwaitBlockLimit {
    
    volatile __block bool testPassed = true;
    volatile __block _Atomic int32_t count = 0;
    
    uint32_t limit = 4;
    dispatch_queue_limited_t dql = dispatch_queue_limited_create(limit, QOS_CLASS_DEFAULT);
    dispatch_queue_t dq = dispatch_queue_create("", DISPATCH_QUEUE_SERIAL);
    
    for (int i = 1; i < 10; i++) {
        dispatch_queue_limited_enqueue_await(dql, ^{
            int32_t c = atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
            if (c >= limit || !testPassed) {
                testPassed = false;
            }
            
            dispatch_async(dq, ^{
                printf("thread %d: count = %d\n", i, c+1);
                usleep(5000);
                atomic_fetch_sub_explicit(&count, 1, memory_order_relaxed);
                dispatch_queue_limited_dequeue_await(dql);
            });
        });
    }
    
    sleep(1);
    XCTAssertTrue(testPassed);
}

- (void)testConcurAwaitBlockLimit {
    
    volatile __block bool testPassed = true;
    volatile __block _Atomic int32_t count = 0;
    
    uint32_t limit = 4;
    dispatch_queue_limited_t dql = dispatch_queue_limited_create(limit, QOS_CLASS_DEFAULT);
    
    for (int i = 1; i < 10; i++) {
        dispatch_queue_limited_enqueue_await(dql, ^{
            int32_t c = atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
            if (c >= limit || !testPassed) {
                testPassed = false;
            }
            
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
                printf("thread %d: count = %d\n", i, c+1);
                usleep(5000);
                atomic_fetch_sub_explicit(&count, 1, memory_order_relaxed);
                dispatch_queue_limited_dequeue_await(dql);
            });
        });
    }
    
    sleep(1);
    XCTAssertTrue(testPassed);
}

// Test to make sure the result of `testConcurAwaitBlockLimit` is valid
- (void)testConcurAwaitBlockLimit_Fail {
    
    volatile __block bool testPassed = true;
    volatile __block _Atomic int32_t count = 0;
    
    uint32_t limit = 4;
    dispatch_queue_limited_t dql = dispatch_queue_limited_create(limit, QOS_CLASS_DEFAULT);
    
    for (int i = 1; i < 10; i++) {
        dispatch_queue_limited_enqueue(dql, ^{
            int32_t c = atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
            if (c >= limit || !testPassed) {
                testPassed = false;
            }
            
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
                printf("thread %d: count = %d\n", i, c+1);
                usleep(5000);
                atomic_fetch_sub_explicit(&count, 1, memory_order_relaxed);
            });
        });
    }
    
    sleep(1);
    XCTAssertFalse(testPassed);
}

- (void)testTargetQueue {
    uint32_t limit = 4;
    dispatch_queue_limited_t dql = dispatch_queue_limited_create(limit, QOS_CLASS_DEFAULT);
    dispatch_queue_t dq = dispatch_queue_create("", DISPATCH_QUEUE_SERIAL);
    
    dispatch_queue_limited_set_target_queue(dql, dq);
    
    dispatch_queue_limited_enqueue(dql, ^{
        dispatch_assert_queue(dq);
    });
    
    usleep(1000);
    
    // dispatch_assert_queue will crash if not true
    XCTAssertTrue(1);
}

@end
