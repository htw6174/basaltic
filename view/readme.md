The project in this folder defines how the UiState and WorldState are displayed on-screen.

Must include a file named "basaltic_view.h" that includes `../model/basaltic_model.h` and declares these methods:
```
bc_ViewContext* bc_view_setup(bc_RenderContext*);
void bc_view_teardown(bc_ViewContext*);
u32 bc_view_drawFrame(bc_ViewContext*, bc_ModelData*, bc_RenderContext*, bc_CommandQueue);

bc_EditorContext* bc_view_setupEditor();
void bc_view_teardownEditor(bc_EditorContext*);
u32 bc_view_drawEditor(bc_EditorContext*, bc_ViewContext*, bc_ModelData*, bc_CommandQueue);
```

And defines these structs:
```
bc_ViewContext;
bc_EditorContext;
```

You probably want to configure the cmake project here to include the `../include` and `../model` directories
