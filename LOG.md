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

## 🌌 Day 04: Keep-alive

**Date:** 2025-01-15  
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

Today I learned about `select` and `poll` system calls, which are used for multiplexing I/O. It was quite challenging to understand how they work. And today a few unexpected things happened... it was my first retreat, after all, so some issues weren't handled perfectly. But I learned a lot from the experience. And I can say proudly that the mission was essentially completed. I'll patch the issues tomorrow and it'll be fine. 

---

## 🌠 Day 05: Refactor with epoll

**Date:** 2025-01-16  
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

The function of `epoll` is similar to `select` and `poll`, but it is more efficient for handling a large number of file descriptors. But they're all difficult to understand. However, there is no denying that all of them are very elegant. I have to admire the designers of these system calls again. They are truly geniuses.

OK, today's summary:
1. `epoll` is more efficient than `select` and `poll` for handling a large number of file descriptors. And its logic is more simple than `select` and `poll`.👍 Actually, having two different logics in my mind is making me a bit confused.😵
2.  Be careful when coding!!! Maybe a small typo can lead to big problems.😓

---

## 🌌 Day 06: Write Buffer

**Date:** 2025-01-17  
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

*"In summary, we should always try to use the fewest number of system calls necessary to get the job done."* -- This sentence from *"Unix Network Programming"* really enlightened me today. It made me realize the importance of efficiency in system programming and I will keep it in mind moving forward.

Author day full of insights:
1. Buffering is crucial for performance. By minimizing the number of write system calls, we can significantly improve throughput.📈
2. The more I learn, the more I realize how difficult system programming is. I need more time to design functions and more time to understand the underlying principles.⌛Sometimes a little function or a small structure can contain a lot of knowledge.🤯
3. And importantly, be careful when coding!!! I made a little typo again and it caused a big problem.😓
4. Oh, it's worth mentioning that when handling a new connection, don't forget to upadte `&ev` structure before calling `epoll_ctl`. Otherwise, it may lead to unexpected behavior. Remember that the array `events` is read-only.💀 

---

## 🌠 Day 07: Memory poll

**Date:** 2025-01-18  
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

Today is not special. And everything goes well. I achieved dynamically allocated buffer pool for each connection. It works well.

Today's summary:
1. Memory pool can significantly reduce memory allocation overhead and fragmentation. By reusing pre-allocated memory blocks, we can improve performance and reduce latency.🚀
2. You must have a deep understanding and thorough mastery of every function you use in your code -- its purpose, parameters, return values, and potential pitfalls. This is crucial for writing robust and efficient code.📚

---

## 🌌 Day 08: Zero-copy

**Date:** 2025-01-19
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

A normal day... Although zero-copy is today's theme, I still reviewed the design of tcmalloc and made some optimizations. Wow, it's really amazing.

Today's summary: not so much special things...

---

## 🌠 Day 09: Load Testing & Optimization

**Date:** 2025-01-20
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

Oh, today is snowing outside. It's rare in this city. I like it. It makes me feel peaceful. Actually, snowing days are always good for coding. A normal day as before...

Today's summary:
1. The scale of code is determined by states and functions, while the size of the code is determined by error handling. This is a profound insight that I learned today. I️t will affect me in the future. Just as the saying goes, "It takes 10% of the effect to write the a program that runs, while 90% to make it robust."🛠️
2. I don't know how the veteran programmers think so thoroughly about error handling and designing the states and functions. Maybe it's accumulated from years of experience. I have to learn more and practice more from them. They are really masters.👑

---

## 🌠 Day 10: Duplex communication

**Date:** 2025-01-21
**Status:** Systems Green.

**Log:**

Fix some bugs left from yesterday, and add some text comments to the code for better understanding.

*How can a wandering sun have any cares?*

A normal day as before...

Today's summary:
1. Duplex communication is essential for modern web applications. It allows for real-time data exchange between clients and servers, enabling features like live updates and interactive experiences.🌐
2. Implementing duplex communication requires careful consideration of protocols, data formats, and performance optimizations. It's a complex but rewarding endeavor.⚙️

---

<div align="center">

[PLAN](PLAN.md) | [LOG](LOG.md) | [README](README.md)

MIT License © 2026 Howell Stark

</div>