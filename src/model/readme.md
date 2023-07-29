The project in this folder defines the background logic for the application that runs on a separate thread.

Must include a file named "basaltic_model.h" that declares these methods:
```
int bc_model_start(void*);
```

And defines these structs:
```
bc_ModelData;
bc_ModelSetupSettings;
```

The signature of bc_model_start corresponds to the type SDL_ThreadFunction, and the void* supplied will always be of type bc_LogicThreadInput*.
bc_model_start is responsible for creating modelData based on modelSetupSettings and writing a pointer to that data into the value of `*logicThreadInput->modelData`
If running in a continuous loop, the Model must not complete a loop in less than `logicThreadInput->interval` milliseconds
If using `logicThreadInput->inputQueue`, the Model should only interact with the queue through `basaltic_commandQueue` methods, and should not block the queue longer than necessary (recommended usage is to only use `bc_commandQueueIsEmpty` and `bc_transferCommandQueue` on the inputQueue, and process commands on a secondary queue).

You probably want to configure the cmake project here to include the `../include` directory for core engine definitions
