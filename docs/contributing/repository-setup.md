# Repository setup

One-time configuration of the GitHub repository, done by an administrator. Developers do not need any of
this.

## Format bot

The *Format* workflow fixes unformatted C++ code in pull requests by itself: it runs clang-format, commits the
result to the branch, and comments on the pull request. To push commits that trigger the other workflows
(*Build*, *Format*), it uses its own identity, a **GitHub App**, instead of the default workflow token.

Without this setup, the workflow still runs but only **checks** the formatting and fails when files need to
be reformatted.

### 1. Create the GitHub App

Open [**Settings → Developer settings → GitHub Apps → New GitHub App**](https://github.com/settings/apps/new)
on the account that owns the repository, and fill in:

| Field | Value |
|---|---|
| **GitHub App name** | A unique name, for example `rtype-format-bot` (it appears as `rtype-format-bot[bot]` on commits) |
| **Homepage URL** | `https://github.com/Akinator31/Rtype` |
| **Webhook → Active** | Unchecked |
| **Repository permissions → Contents** | Read and write |
| **Repository permissions → Pull requests** | Read and write |
| **Where can this GitHub App be installed?** | Only on this account |

Click **Create GitHub App**.

### 2. Get its credentials

On the page of the new app:

1. Copy the **Client ID** (starts with `Iv`).
2. In **Private keys**, click **Generate a private key**: a `.pem` file is downloaded.

### 3. Install it on the repository

In the left menu of the app, **Install App** → **Install** next to the account → **Only select repositories**
→ `Rtype` → **Install**.

### 4. Give the credentials to the workflow

In the repository: **Settings → Secrets and variables → Actions**.

| Tab | Name | Value |
|---|---|---|
| **Variables** | `FORMAT_BOT_CLIENT_ID` | The Client ID |
| **Secrets** | `FORMAT_BOT_PRIVATE_KEY` | The whole content of the `.pem` file, including the `BEGIN` and `END` lines |

Then delete the `.pem` file from your computer: a new key can be generated at any time from the app page.

### 5. Check

Open a pull request with a badly formatted C++ file. The *Format* workflow should push a commit
`style: apply clang-format` signed by the bot, comment on the pull request, and the *Build* workflow should
run again on that commit.

??? question "The workflow still only checks the formatting"

    The bot is used only for pull requests opened **from a branch of this repository** (not from a fork),
    and only if the variable `FORMAT_BOT_CLIENT_ID` exists. Check its exact name in the **Variables** tab
    (not *Secrets*).

??? question "The push of the bot is rejected"

    Check that the app is installed on the repository (step 3) with **Contents: Read and write**. If the
    permissions were changed after installation, accept the new permissions in
    **Settings → Applications → Installed GitHub Apps**.

## Documentation site

**Settings → Pages → Build and deployment → Source**: select **GitHub Actions**. The *Documentation* workflow
deploys the site on every change to `main`.

GitHub Pages on a private repository requires a paid plan (GitHub Pro, Team, or the GitHub Student Developer
Pack).

## Protection of `main`

Recommended: **Settings → Rules → Rulesets → New branch ruleset**, targeting the default branch, with:

- **Require a pull request before merging**, with at least one approval;
- **Require status checks to pass**: `Build (linux)`, `Build (windows)`, `Sanitizers (address,undefined)`,
  `clang-format`;
- **Block force pushes**.

Status checks can be selected only after they have run once, so add them after the first pull request.
The *Documentation* checks are not required: that workflow only runs when the documentation or the code
changes.
