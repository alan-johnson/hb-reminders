#!/usr/bin/env node

/**
 * Example test script for the Unified Task Server
 * This demonstrates how to use the API with Apple Reminders
 */

const BASE_URL = 'http://localhost:3000';

async function testAPI() {
  console.log('🧪 Testing Unified Task Server API\n');

  try {
    // 1. Check server health
    console.log('1️⃣ Checking server health...');
    const healthResponse = await fetch(`${BASE_URL}/health`);
    const health = await healthResponse.json();
    console.log('✅ Server is healthy:', health);
    console.log('');

    // 2. Get available providers
    console.log('2️⃣ Getting available providers...');
    const providersResponse = await fetch(`${BASE_URL}/api/providers`);
    const providers = await providersResponse.json();
    console.log('✅ Available providers:', providers);
    console.log('');

    // 3. Get all task lists (using Apple Reminders)
    console.log('3️⃣ Getting all task lists (Apple Reminders)...');
    const listsResponse = await fetch(`${BASE_URL}/api/lists?provider=apple`);
    const listsData = await listsResponse.json();
    console.log('✅ Task lists:', JSON.stringify(listsData, null, 2));
    console.log('');

    if (listsData.lists && listsData.lists.length > 0) {
      const firstList = listsData.lists[0];
      
      // 4. Get tasks from the first list
      console.log(`4️⃣ Getting tasks from list "${firstList.name}"...`);
      const tasksResponse = await fetch(
        `${BASE_URL}/api/lists/${encodeURIComponent(firstList.id)}/tasks?provider=apple`
      );
      const tasksData = await tasksResponse.json();
      console.log('✅ Tasks:', JSON.stringify(tasksData, null, 2));
      console.log('');

      // 5. Create a new task
      console.log('5️⃣ Creating a new test task...');
      const createResponse = await fetch(
        `${BASE_URL}/api/lists/${encodeURIComponent(firstList.id)}/tasks?provider=apple`,
        {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            name: 'Test Task from API',
            notes: 'This task was created via the REST API at ' + new Date().toISOString()
          })
        }
      );
      const createData = await createResponse.json();
      console.log('✅ Created task:', JSON.stringify(createData, null, 2));
      console.log('');

      if (createData.task && createData.task.id) {
        // 6. Get task details
        console.log('6️⃣ Getting task details...');
        const taskDetailResponse = await fetch(
          `${BASE_URL}/api/lists/${encodeURIComponent(firstList.id)}/tasks/${encodeURIComponent(createData.task.id)}?provider=apple`
        );
        const taskDetail = await taskDetailResponse.json();
        console.log('✅ Task details:', JSON.stringify(taskDetail, null, 2));
        console.log('');

        // 7. Mark task as complete
        console.log('7️⃣ Marking task as complete...');
        const completeResponse = await fetch(
          `${BASE_URL}/api/lists/${encodeURIComponent(firstList.id)}/tasks/${encodeURIComponent(createData.task.id)}/complete?provider=apple`,
          { method: 'PATCH' }
        );
        const completeData = await completeResponse.json();
        console.log('✅ Task completed:', JSON.stringify(completeData, null, 2));
        console.log('');
      }

      // 8. Get updated tasks list
      console.log('8️⃣ Getting updated tasks list...');
      const updatedTasksResponse = await fetch(
        `${BASE_URL}/api/lists/${encodeURIComponent(firstList.id)}/tasks?provider=apple`
      );
      const updatedTasksData = await updatedTasksResponse.json();
      console.log('✅ Updated tasks:', JSON.stringify(updatedTasksData, null, 2));
    } else {
      console.log('⚠️ No task lists found. Please create a list in Apple Reminders first.');
    }

    console.log('\n🎉 All tests completed successfully!');
    
  } catch (error) {
    console.error('❌ Error:', error.message);
    console.error('\nMake sure the server is running: npm start');
  }
}

// Run the tests
testAPI();
