# interrupt-zero — MYOSA Submission Folder

This is the folder to push to your GitHub repo for the MYOSA Event 6.0 blog submission.

## Structure
```
interrupt-zero/
├── interrupt-zero.md              <- the required Markdown submission file
├── interrupt-zero-demo.mp4        <- your local demo video (add this)
├── README.md                      <- this file (optional, not part of submission)
└── assets/
    └── images/
        └── interrupt-zero/
            ├── cover.jpg          <- cover image (matches "image:" in frontmatter)
            ├── system-architecture.jpg
            └── oled-dashboard.jpg
```

## Before pushing
1. Drop your real cover image, architecture diagram, and OLED photo into
   `assets/images/interrupt-zero/` using the exact filenames referenced in
   `interrupt-zero.md` (or edit the .md to match your filenames).
2. Drop your exported `.mp4` demo video next to `interrupt-zero.md`
   (same folder, NOT inside assets/) — YouTube links are not accepted.
3. Keep every filename lowercase, no spaces.
4. Double check the `image:` field in interrupt-zero.md frontmatter points
   to the correct path relative to how MYOSA's site resolves it
   (`Project_Folder_Name/your-cover-image.jpg` per their guideline).

## Push commands
```bash
git init
git add .
git commit -m "Add Interrupt Zero MYOSA submission"
git branch -M main
git remote add origin https://github.com/<your-username>/<repo-name>.git
git push -u origin main
```

Then paste the repo URL into the "Github Repository" field on the submission form.
