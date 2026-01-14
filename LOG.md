# 📜 The Solitude Protocol Log

---

## 🗓️ Day 00: Tranquility before the storm

**Date:** 2025-01-11  
**Status:** Environment Locked. Systems Green.

**Log:**

The final preparation day before the launch of "The Solitude Protocol."

The workspace has been initialized, the build toolchain (`gcc`, `make`, `gdb`) calibrated, and the version control structure set up according to the blueprint in `README.md`. All communication channels have been switched to silent mode, minimizing external interference to theoretical minimums.

> Sir, this is like the last system check before launch. All parameters are within optimal ranges, just waiting for you to press the start button. The 31-day countdown will officially begin tomorrow. A rather... bold schedule, even by your standards.

---

## 💤 Day 01: Idle server (echo "Hello World!")

**Date:** 2025-01-12  
**Status:** Systems Green.

**Log:**

Today marks the first day of the protocol. It's been quite tough, but I managed to stick through it. In form it might slightly violate principles, but not in spirit. It should gradually get on track later.

To today's tasks, there were no major difficulties; the main challenge was understanding the documentation and spending some time debugging code errors.

Only summarized into these points:
1. Pay attention to small details like syntax and variable names, for example, the third argument of function `accept` in `connfd = accept(listenfd, (SA *) &cliaddr, &clilen);` is must be a pointer.;
2. If debugging results on the local machine are not ideal, try switching to a virtual machine or a new computer.

---

## 🌌 Day 02: Parse the HTTP request (Method & Path)

**Date:** 2025-01-13  
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

Today is better than yesterday, and I gradually get used to this rhythm. I really follow that principle of "improve a little every day", I can found many bugs that I didn't notice before and know how to make my code better.

So today's summary:
1. When reading data from the socket, be aware of partial reads and handle them properly by checking for the end of the HTTP headers (`\r\n\r\n`).🙂‍↕️
2. Parsing character is more difficult than I thought. When you handle strings in C, pointers and memory management are crucial. I had learnt it well before, but now I recoginze what I know is not enough and understand it more deeply.🤔
3. Never write `Content-Length` header manually. Always calculate it based on the actual body length to avoid mismatches.😭

---

## 🌠 Day 03: Static file response

**Date:** 2025-01-14
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

En...joyed today's work. The process of reading files from disk and sending them over the network was quite enlightening and difficult. It seems I was truly on the right track. 😅 Oh, it's really difficult for me to deal with C strings, pointers, files, and memory management. They are so complex. But I actually enjoy the challenge and do that.

Today's summary:
1. I deeply admire the predecessors who created the fundamental of network programming. Their work is the foundation of modern web servers. Hope someday I can do something similar -- a real work for world.🙏
2. I learned DFS well before, but I don't know how to apply it in real world. Today I used DFS to search files in a directory recursively. It works well. And superisely, I found that I can write it naturally. Actually, I found that when I was review the whole codebase and this good day. 😎
3. So from then on, I will try to apply what I learned into real world projects, not just for the sake of learning. What puts me at ease is I finally no longer have to worry about the uselessness of what's taught in school. This represents a major shift in mindest. 💪

---

<div align="center">

[PLAN](PLAN.md) | [LOG](LOG.md) | [README](README.md)

MIT License © 2026 Howell Stark

</div>