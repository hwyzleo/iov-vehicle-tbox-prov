#include <gtest/gtest.h>
#include "prov_context.h"
#include "log.h"

namespace tbox::prov {
namespace {

TEST(ContextScopeTest, SetsAndClearsContext) {
    std::string trace_id = "trace-123";
    std::string request_id = "req-456";
    std::string session_id = "sess-789";
    
    // 初始状态应为空
    auto* initial = tbox::fw::log::ContextScope::current();
    EXPECT_EQ(initial, nullptr);
    
    {
        auto scope = make_context_scope(trace_id, request_id, session_id);
        
        auto* current = tbox::fw::log::ContextScope::current();
        ASSERT_NE(current, nullptr);
        EXPECT_EQ(current->trace_id, trace_id);
        EXPECT_EQ(current->request_id, request_id);
        EXPECT_EQ(current->session_id, session_id);
    }
    
    // ContextScope 离开作用域后应清理上下文
    auto* after = tbox::fw::log::ContextScope::current();
    EXPECT_EQ(after, nullptr);
}

TEST(ContextScopeTest, NoSessionId) {
    std::string trace_id = "trace-123";
    std::string request_id = "req-456";
    
    {
        auto scope = make_context_scope(trace_id, request_id);
        
        auto* current = tbox::fw::log::ContextScope::current();
        ASSERT_NE(current, nullptr);
        EXPECT_EQ(current->trace_id, trace_id);
        EXPECT_EQ(current->request_id, request_id);
        EXPECT_TRUE(current->session_id.empty());
    }
}

TEST(ContextScopeTest, GenerateRequestId) {
    std::string id1 = generate_request_id();
    std::string id2 = generate_request_id();
    
    // 两次生成的 ID 应不同
    EXPECT_NE(id1, id2);
    
    // ID 应以 "prov-" 开头
    EXPECT_TRUE(id1.substr(0, 5) == "prov-");
    EXPECT_TRUE(id2.substr(0, 5) == "prov-");
}

TEST(ContextScopeTest, NestedScopes) {
    std::string outer_trace = "outer-trace";
    std::string outer_request = "outer-req";
    std::string inner_trace = "inner-trace";
    std::string inner_request = "inner-req";
    
    {
        auto outer_scope = make_context_scope(outer_trace, outer_request);
        
        auto* current = tbox::fw::log::ContextScope::current();
        ASSERT_NE(current, nullptr);
        EXPECT_EQ(current->trace_id, outer_trace);
        EXPECT_EQ(current->request_id, outer_request);
        
        {
            auto inner_scope = make_context_scope(inner_trace, inner_request);
            
            current = tbox::fw::log::ContextScope::current();
            ASSERT_NE(current, nullptr);
            EXPECT_EQ(current->trace_id, inner_trace);
            EXPECT_EQ(current->request_id, inner_request);
        }
        
        // 内层 scope 结束后，应恢复外层上下文
        current = tbox::fw::log::ContextScope::current();
        ASSERT_NE(current, nullptr);
        EXPECT_EQ(current->trace_id, outer_trace);
        EXPECT_EQ(current->request_id, outer_request);
    }
}

} // namespace
} // namespace tbox::prov
