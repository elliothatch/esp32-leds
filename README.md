# LEDs

can fit about 1100 frames of pixel data for 8x8 (290KB)

# Architecture
Animations, layer composition, and LED buffer write are performed in a dedicated task, called the LED task.

Questions: how should pixel updates be communicated to the LED task?
 - Command based: Queue of "draw" commands. Layers/frames are allocated with a command which returns an integer id. Commands like set_pixel(id, x, y, color), set_rect(id, x, y, width, height, color/colorArray) copy data to the queue, which is read by the LED task and composited into the pixel buffer
    - Pros:
       - data protection, partial updates less problematic (e.g. app task modifying pixel buffer as it is being written).
	   - rendering task has full control of memory and can potentially optimize data allocation, access, caching, etc.
    - Cons:
       - more copies and RAM use
       - partial updates still possible (set_pixel called twice, only first is queued before rendering, resulting in partial update. can resolve by supporting command groups i.e. transactions.age
       - difficult/limited support for reading existing buffers (e.g. to shift a pixel to another position.
       - requires implementing many commands
 - Buffer based: App task owns all buffers, layers, frames. Commands are queued to add/remove layers by reference
 	 - Pros:
 	 	 - More control/flexibility 
 	 	 - Less resource use
 	 - Cons:
 	     - Sync issues/partial updates possible. Render task might begin while updating a buffer.
 	     - Possible data sharing issues. Moving or changing address of buffer can break things

 - Buffer based, with update commands. Same as buffer based, but composition is only performed when triggered by "update" commands. Data sync issues still exist when compositing multiple layers, but if none of the layers have changed, no compositing necessary. Intermediate rendering steps stored in internal buffer?
   - Pros:
      - reduces unnecessary rendering time
      - solve some data sync issues (but not all)
   - Cons:
   	  - potentially uses a lot of RAM if there are many compositing steps
   	  - more complexity
   	  - command based method can also implement these optimizations (probably better too)

