# Google Tasks Provider

A provider for the Task Server that interfaces with Google Tasks using the Google Tasks API.

## Overview

The `google` provider uses the Google Tasks API to access tasks from Google Workspace or personal Google accounts. It supports OAuth 2.0 authentication and provides access to all task lists in a user's Google account.

## System Requirements

- **Node.js**: Version 14 or higher
- **Google Account**: Personal Gmail account or Google Workspace account
- **Google Cloud Project**: Required for OAuth authentication
- **Internet Connection**: Required for API access

## Installation & Setup

### 1. Create a Google Cloud Project

1. **Go to Google Cloud Console**
   - Visit [https://console.cloud.google.com](https://console.cloud.google.com)
   - Sign in with your Google account

2. **Create New Project**
   - Click the project dropdown at the top
   - Click **New Project**
   - Project name: "Task Server" (or your preferred name)
   - Click **Create**
   - Wait for project creation, then select it

### 2. Enable Google Tasks API

1. **Navigate to API Library**
   - In the left sidebar, go to **APIs & Services** > **Library**
   - Search for "Google Tasks API"
   - Click on **Google Tasks API**

2. **Enable the API**
   - Click **Enable**
   - Wait for activation (may take a minute)

### 3. Configure OAuth Consent Screen

1. **Go to OAuth Consent Screen**
   - Navigate to **APIs & Services** > **OAuth consent screen**

2. **Choose User Type**
   - **External**: For personal use or public apps
   - **Internal**: For Google Workspace organizations only
   - Click **Create**

3. **Fill Out App Information**
   - **App name**: "Task Server"
   - **User support email**: Your email
   - **Developer contact**: Your email
   - Click **Save and Continue**

4. **Add Scopes**
   - Click **Add or Remove Scopes**
   - Find and select: `https://www.googleapis.com/auth/tasks`
   - Click **Update**
   - Click **Save and Continue**

5. **Add Test Users** (if External)
   - Click **Add Users**
   - Add your Google account email
   - Click **Save and Continue**

6. **Review and Submit**
   - Review your settings
   - Click **Back to Dashboard**

### 4. Create OAuth 2.0 Credentials

1. **Go to Credentials**
   - Navigate to **APIs & Services** > **Credentials**
   - Click **Create Credentials** > **OAuth client ID**

2. **Configure OAuth Client**
   - **Application type**: Web application
   - **Name**: "Task Server Web Client"

3. **Add Authorized Redirect URIs**
   - Click **Add URI** under "Authorized redirect URIs"
   - Enter: `http://localhost:3000/auth/google/callback`
   - Click **Create**

4. **Save Credentials**
   - Copy the **Client ID**
   - Copy the **Client Secret**
   - Click **OK**

### 5. Configure Environment Variables

Create or update your `.env` file:

```bash
# Google Configuration
GOOGLE_CLIENT_ID=your_client_id_here.apps.googleusercontent.com
GOOGLE_CLIENT_SECRET=your_client_secret_here
GOOGLE_REDIRECT_URI=http://localhost:3000/auth/google/callback

# Optional: Set as default provider
DEFAULT_PROVIDER=google
```

### 6. Install Dependencies

```bash
cd task-server-local
npm install
```

### 7. Start the Server

```bash
npm start
```

## Authentication

Google Tasks requires OAuth 2.0 authentication. Follow these steps:

### Step 1: Get Authorization URL

```bash
curl http://localhost:3000/auth/google/url
```

**Response:**
```json
{
  "authUrl": "https://accounts.google.com/o/oauth2/v2/auth?client_id=...&redirect_uri=...&scope=...&response_type=code"
}
```

### Step 2: Authorize in Browser

1. Copy the `authUrl` from the response
2. Open it in your web browser
3. Sign in to your Google account (if not already)
4. Review the permissions requested
5. Click **Allow** to grant access

### Step 3: Get Session ID

After authorization, you'll be redirected to:
```
http://localhost:3000/auth/google/callback?code=...
```

The page will display your session ID:
```json
{
  "success": true,
  "sessionId": "xyz789abc123",
  "message": "Google authentication successful"
}
```

**Save this session ID** - you'll need it for all API requests.

### Step 4: Use Session ID

Include the session ID in the `X-Session-ID` header:

```bash
curl -H "X-Session-ID: xyz789abc123" \
  "http://localhost:3000/api/lists?provider=google"
```

## Usage

### Available API Endpoints

#### 1. Get Authorization URL
```bash
GET /auth/google/url
```

**Response:**
```json
{
  "authUrl": "https://accounts.google.com/o/oauth2/v2/auth?..."
}
```

#### 2. OAuth Callback
```
GET /auth/google/callback?code=...
```

This endpoint is called automatically after OAuth authorization. It returns the session ID.

#### 3. Get All Task Lists
```bash
GET /api/lists?provider=google
X-Session-ID: xyz789abc123
```

**Response:**
```json
{
  "provider": "google",
  "lists": [
    {
      "id": "MTIzNDU2Nzg5MDEyMzQ1Njc4OTA",
      "name": "My Tasks"
    },
    {
      "id": "OTg3NjU0MzIxMDk4NzY1NDMyMTA",
      "name": "Work"
    }
  ]
}
```

#### 4. Get Tasks from a List
```bash
GET /api/lists/:listId/tasks?provider=google
X-Session-ID: xyz789abc123
```

**Query Parameters:**
- `showCompleted` (boolean): Include completed tasks (default: false)
- `limit` (number): Maximum number of tasks to return (default: 50)

**Response:**
```json
{
  "provider": "google",
  "listId": "MTIzNDU2Nzg5MDEyMzQ1Njc4OTA",
  "count": 2,
  "limit": 50,
  "showCompleted": false,
  "tasks": [
    {
      "id": "MTIzNDU2Nzg5MDEyMzQ1Njc4OTAxMjM0",
      "name": "Write report",
      "completed": false,
      "notes": "Q4 quarterly report",
      "dueDate": "2026-02-15T00:00:00.000Z"
    }
  ]
}
```

#### 5. Get Task Details
```bash
GET /api/lists/:listId/tasks/:taskId?provider=google
X-Session-ID: xyz789abc123
```

#### 6. Complete a Task
```bash
PATCH /api/lists/:listId/tasks/:taskId/complete?provider=google
X-Session-ID: xyz789abc123
```

**Response:**
```json
{
  "provider": "google",
  "listId": "MTIzNDU2Nzg5MDEyMzQ1Njc4OTA",
  "taskId": "MTIzNDU2Nzg5MDEyMzQ1Njc4OTAxMjM0",
  "success": true,
  "message": "Task marked as complete"
}
```

#### 7. Create a New Task
```bash
POST /api/lists/:listId/tasks?provider=google
X-Session-ID: xyz789abc123
Content-Type: application/json

{
  "name": "Review pull request",
  "notes": "PR #456 - new feature",
  "dueDate": "2026-02-18T00:00:00.000Z"
}
```

**Response:**
```json
{
  "provider": "google",
  "listId": "MTIzNDU2Nzg5MDEyMzQ1Njc4OTA",
  "task": {
    "id": "MTIzNDU2Nzg5MDEyMzQ1Njc4OTAxMjM1",
    "name": "Review pull request"
  }
}
```

## How It Works

### Google Tasks API

The provider uses these Google Tasks API endpoints:

- **Lists**: `GET https://tasks.googleapis.com/tasks/v1/users/@me/lists`
- **Tasks**: `GET https://tasks.googleapis.com/tasks/v1/lists/{listId}/tasks`
- **Update**: `PATCH https://tasks.googleapis.com/tasks/v1/lists/{listId}/tasks/{taskId}`
- **Insert**: `POST https://tasks.googleapis.com/tasks/v1/lists/{listId}/tasks`

### OAuth 2.0 Flow

1. User requests authorization URL from server
2. User visits URL and grants permission in browser
3. Google redirects back with authorization code
4. Server exchanges code for access + refresh tokens
5. Tokens are stored in session
6. Access token is used for all API requests
7. Refresh token is used to get new access tokens when expired

### ID Format

Google uses Base64-encoded IDs:
- **List ID**: `MTIzNDU2Nzg5MDEyMzQ1Njc4OTA`
- **Task ID**: `MTIzNDU2Nzg5MDEyMzQ1Njc4OTAxMjM0`

### Date Format

Google Tasks uses RFC 3339 timestamps:
```
2026-02-15T00:00:00.000Z
```

### Token Management

- **Access Token**: Expires after 1 hour
- **Refresh Token**: Long-lived, used to get new access tokens
- **Automatic Refresh**: Provider automatically refreshes expired tokens

## Limitations

1. **OAuth Required**: Cannot work without user authorization
2. **Session-Based**: Sessions stored in memory (lost on restart)
3. **Single User**: Current implementation is single-user
4. **Rate Limiting**: Google applies rate limits (typically 50,000 requests/day)
5. **Test Users Only**: If app is not verified, only test users can authenticate

## Troubleshooting

### "Authentication Required"

**Symptom**: API calls fail with "Authentication required"

**Solutions**:
- Complete OAuth flow at `/auth/google/url`
- Include `X-Session-ID` header in requests
- Re-authenticate if session expired

### "Invalid Grant"

**Symptom**: Authorization fails with "invalid_grant" error

**Causes**:
- Authorization code already used
- Authorization code expired (10 minutes)
- Clock skew between server and Google

**Solutions**:
- Get a new authorization URL and try again
- Check server time is synchronized
- Don't refresh the callback page (generates new auth request)

### "Access Denied"

**Symptom**: OAuth consent screen shows "Access Denied"

**Causes**:
- User is not added to test users list (for External apps)
- Required scope not requested
- App not published (for External apps)

**Solutions**:
- Add user to test users in OAuth consent screen
- Verify scope `https://www.googleapis.com/auth/tasks` is requested
- Consider publishing app or use Internal user type

### "Redirect URI Mismatch"

**Symptom**: OAuth fails with redirect URI mismatch error

**Solutions**:
- Verify `GOOGLE_REDIRECT_URI` matches exactly what's in Google Cloud Console
- Check for http vs https mismatch
- Ensure no trailing slashes
- Common correct URI: `http://localhost:3000/auth/google/callback`

### "API Not Enabled"

**Symptom**: Requests fail with API not enabled error

**Solutions**:
- Enable Google Tasks API in Google Cloud Console
- Wait a few minutes for propagation
- Verify correct project is selected

### Token Expired

**Symptom**: Requests suddenly fail after working

**Solutions**:
- Provider should automatically refresh tokens
- If refresh fails, re-authenticate via `/auth/google/url`
- Check refresh token hasn't been revoked

## Best Practices

### Production Deployment

1. **Use Persistent Session Storage**
   ```javascript
   // Replace in-memory storage with Redis/database
   const sessions = new RedisClient();
   ```

2. **Implement Multi-User Support**
   ```javascript
   // Associate sessions with user IDs
   sessions.set(userId, { google: { tokens } });
   ```

3. **Add Token Encryption**
   ```javascript
   // Encrypt tokens before storing
   const encryptedToken = encrypt(accessToken);
   ```

4. **Use HTTPS**
   ```bash
   GOOGLE_REDIRECT_URI=https://yourdomain.com/auth/google/callback
   ```

5. **Handle Token Revocation**
   ```javascript
   // Detect revoked tokens and prompt re-auth
   if (error.code === 401) {
     redirectToAuth();
   }
   ```

### Security

- Store client secrets securely (environment variables, secrets manager)
- Never commit credentials to version control
- Use HTTPS in production
- Implement CSRF protection for OAuth flows
- Validate redirect URIs strictly
- Rotate client secrets periodically
- Monitor for suspicious activity

### Rate Limiting

- Implement request caching where appropriate
- Batch requests when possible
- Add exponential backoff for retries
- Monitor quota usage in Google Cloud Console

## Environment Variables Reference

```bash
# Required
GOOGLE_CLIENT_ID=your_client_id.apps.googleusercontent.com
GOOGLE_CLIENT_SECRET=your_client_secret

# Optional
GOOGLE_REDIRECT_URI=http://localhost:3000/auth/google/callback
DEFAULT_PROVIDER=google
```

## API Reference

### Google Tasks API Documentation

- [Google Tasks API Overview](https://developers.google.com/tasks)
- [API Reference](https://developers.google.com/tasks/reference/rest)
- [OAuth 2.0](https://developers.google.com/identity/protocols/oauth2)
- [Task Resource](https://developers.google.com/tasks/reference/rest/v1/tasks)

### Scopes Required

- `https://www.googleapis.com/auth/tasks` - Read and write access to Google Tasks

## Testing

Use the Google APIs Explorer to test API calls:
- [Google APIs Explorer](https://developers.google.com/apis-explorer/#p/tasks/v1/)

## License

This provider is part of the hb-reminders project.

## Support

For Google-specific issues:
- Check Google Cloud Console configuration
- Verify OAuth consent screen setup
- Review API quotas and limits
- Test with Google APIs Explorer
- Check Google Cloud Platform status
