# Keeping the display safe

The display gets everything through an online "post office" called an **MQTT broker** (EMQX Cloud). Anyone with a login for the broker can try to send things to the display. This guide sets up the logins so each person can do only what they need.

## The short version

There are two "mailboxes" (topics) going to the display and one coming back from it:

| Topic | What goes there | Who may send |
|---|---|---|
| `radio/inbox` | Your messages, settings, GIFs and **software updates** | Only you |
| `radio/messages` | Friends' text messages, with their name. The display only ever shows these as text. | You and your friends |
| `radio/status` | The display's replies (online, settings, "update installed") | Only the display |

Software updates have extra protection: even when an update arrives, the display asks first. It shows **"Software update – Press the upper button to install"** for 60 seconds. If nobody presses the upper button, or someone presses the lower button, the update is thrown away. **If an update ever appears on the screen and you did not send it, press the lower button and change your owner password.**

## The accounts to create

Create one login for each of these. Use a long, different password for each (a password manager can make them).

| Account | Example username | May **publish** (send) to | May **subscribe** (listen) to |
|---|---|---|---|
| The display | `display` | `radio/status` | `radio/inbox`, `radio/messages` |
| You (the owner) | `owner` | `radio/inbox`, `radio/messages` | `radio/status` |
| Each friend | `friend-anna`, `friend-nikos`, … | `radio/messages` | nothing |

**Everything else is denied.**

The display account's login goes into `firmware/include/secrets.h` on the display. Your owner login goes into the control page (`web/tdisplay-control.html`). Each friend gets their own login for `web/tdisplay-messages.html`.

### Why it is set up like this

- **The display's password lives on the device**, so you should treat it as known to whoever has the device. If it leaks, someone could only listen to messages and pretend to be the display's status. They could not install software or change settings, because the display account can't send to `radio/inbox`.
- **Friends must never be able to install software.** Their accounts can only send to `radio/messages`, and the display treats everything there as plain text. It doesn't matter what they type: settings, GIF links and update data sent there just show up as text on the screen.
- **One account per friend** means that if one login leaks, or a friendship ends, you delete just that one account and nobody else is affected.

## Setting it up in EMQX Cloud

These steps follow EMQX's current documentation. The console sometimes changes, so if a button looks different, check the linked pages.

### 1. Create the logins (Authentication)

1. Log in to the [EMQX Cloud console](https://cloud.emqx.com/) and open your deployment.
2. In the left menu, click **Access Control → Authentication**.
3. Click **+ Add**, enter a username and password, and click **Confirm**.
4. Do this for `display`, `owner` and each friend.

To change a password later, click the edit icon on that row. To remove a friend, click the delete icon.

Docs: [Default Authentication](https://docs.emqx.com/en/cloud/latest/deployments/default_auth.html) · [Authentication overview](https://docs.emqx.com/en/cloud/latest/deployments/auth_overview.html)

### 2. Set what each login may do (Authorization)

1. In the left menu, click **Access Control → Authorization**. The page is called *Client Authorization*.
2. Open the **Username** tab and click **+ Add** for each line below. Choose **Allow** as the permission each time and click **Confirm**.

   | Username | Topic | Action |
   |---|---|---|
   | `display` | `radio/inbox` | Subscribe |
   | `display` | `radio/messages` | Subscribe |
   | `display` | `radio/status` | Publish |
   | `owner` | `radio/inbox` | Publish |
   | `owner` | `radio/messages` | Publish |
   | `owner` | `radio/status` | Subscribe |
   | `friend-anna` (one line per friend) | `radio/messages` | Publish |

3. **Deny everything else.** By default, EMQX Cloud allows anything that is not explicitly denied (it calls this "blacklist mode"), so this step matters most.
   - **Serverless** (the free tier): open the **All Users** tab and click **+ Add**. Set Topic to `#`, Action to **Publish & Subscribe** and Permission to **Deny**, then click **Confirm**. `#` means "every topic".
   - **Dedicated** (EMQX 5.10 or later): you can instead click the **Blacklist Mode** button at the top right of the Client Authorization page and switch to **Whitelist Mode**.

   This works because EMQX checks the Username rules first, then the All Users rules, and uses the first rule that matches. Your allow rules win for the exact topics listed, and the deny-everything rule catches everything else.

Docs: [Default Authorization](https://docs.emqx.com/en/cloud/latest/deployments/default_authz.html) · [Authorization overview](https://docs.emqx.com/en/cloud/latest/deployments/authz_overview.html)

### 3. Check it works

- **Owner:** connect with the control page. The "Display" card should show the display as online, which proves `owner` can listen to `radio/status`. Send a message.
- **Friend:** send a message from `tdisplay-messages.html` with a friend login. It should appear with their name.
- **Friend, trying to do too much:** connect to the control page with a *friend* login and press Send on a message (that goes to `radio/inbox`). Nothing should appear on the display, and the display card should stay on "Waiting for the display", because friends can't listen to `radio/status`.

## Other rules

- **Never forward port 80 on your router to the display.** The display has a small web page for uploading GIFs on the home network, with no password. It must stay reachable only from inside the home. Everything from outside goes through the broker, which needs no router changes at all.
- **Keep `include/secrets.h` private.** It holds the Wi-Fi password and the display's broker password. It is excluded from git; never send it to anyone or paste it online.
- **Retained messages.** An MQTT sender can mark a message "retained", and then the broker delivers it again every time the display reconnects. The pages here never do this. If one message keeps reappearing after restarts, someone sent it retained: publish an empty retained message to the same topic from the EMQX console to clear it, and consider deleting that sender's account.
- **If you think a password leaked**, change it under Authentication (edit icon). For the display's password, also update `secrets.h` and upload the firmware again.
