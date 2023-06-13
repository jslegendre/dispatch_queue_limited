import XCTest
@testable import DispatchQueueLimited

final class dispatch_queue_limitedTests: XCTestCase {
    func testExample() throws {
        // This is an example of a functional test case.
        // Use XCTAssert and related functions to verify your tests produce the correct
        // results.
        
        let dq = dispatch_queue_limited_create(4, QOS_CLASS_DEFAULT)
        XCTAssertNotNil(dq)
    }
}
