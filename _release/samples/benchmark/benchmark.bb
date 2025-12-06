; OpenGL Performance Benchmark
; Tests rendering performance with multiple objects

Graphics3D 800,600,0,2
SetBuffer BackBuffer()

; Setup camera
cam = CreateCamera()
PositionEntity cam, 0, 5, -20

; Setup light
light = CreateLight()
TurnEntity light, 45, 45, 0

; Configuration
Const NUM_OBJECTS = 100
Const TEST_FRAMES = 500

; Create objects array
Dim cubes(NUM_OBJECTS)
Dim spheres(NUM_OBJECTS)

; Create textured brush
tex = CreateTexture(64, 64)
SetBuffer TextureBuffer(tex)
Color 255, 128, 0 : Rect 0, 0, 32, 32
Color 0, 128, 255 : Rect 32, 0, 32, 32
Color 0, 128, 255 : Rect 0, 32, 32, 32
Color 255, 128, 0 : Rect 32, 32, 32, 32
SetBuffer BackBuffer()

brush = CreateBrush()
BrushTexture brush, tex

; Create cubes
For i = 0 To NUM_OBJECTS - 1
    cubes(i) = CreateCube()
    PaintEntity cubes(i), brush
    x# = (Rnd(2) - 1) * 15
    y# = (Rnd(2) - 1) * 10
    z# = Rnd(1) * 20
    PositionEntity cubes(i), x, y, z
    ScaleEntity cubes(i), 0.5, 0.5, 0.5
Next

; Create spheres
For i = 0 To NUM_OBJECTS - 1
    spheres(i) = CreateSphere(8)
    PaintEntity spheres(i), brush
    x# = (Rnd(2) - 1) * 15
    y# = (Rnd(2) - 1) * 10
    z# = Rnd(1) * 20
    PositionEntity spheres(i), x, y, z
    ScaleEntity spheres(i), 0.3, 0.3, 0.3
Next

; Benchmark loop
Print "Starting benchmark..."
Print "Objects: " + (NUM_OBJECTS * 2)
Print "Frames: " + TEST_FRAMES

startTime = MilliSecs()
frameCount = 0

While frameCount < TEST_FRAMES
    ; Rotate all objects
    For i = 0 To NUM_OBJECTS - 1
        TurnEntity cubes(i), 1, 2, 0.5
        TurnEntity spheres(i), 0.5, 1, 2
    Next

    ; Render
    RenderWorld
    Flip False ; Don't wait for vsync

    frameCount = frameCount + 1

    If KeyHit(1) Then Exit
Wend

endTime = MilliSecs()

; Calculate results
totalTime = endTime - startTime
avgFrameTime# = Float(totalTime) / Float(TEST_FRAMES)
fps# = 1000.0 / avgFrameTime

; Display results
Cls
Color 255, 255, 255

Print ""
Print "=== BENCHMARK RESULTS ==="
Print ""
Print "Total objects: " + (NUM_OBJECTS * 2)
Print "Total frames: " + TEST_FRAMES
Print "Total time: " + totalTime + " ms"
Print "Avg frame time: " + avgFrameTime + " ms"
Print "Average FPS: " + fps
Print ""
Print "Press any key to exit..."

; Write results to file
file = WriteFile("benchmark_results.txt")
WriteLine file, "=== BENCHMARK RESULTS ==="
WriteLine file, "Objects: " + (NUM_OBJECTS * 2)
WriteLine file, "Frames: " + TEST_FRAMES
WriteLine file, "Total time: " + totalTime + " ms"
WriteLine file, "Avg frame time: " + avgFrameTime + " ms"
WriteLine file, "Average FPS: " + fps
CloseFile file

Print "Results saved to benchmark_results.txt"

Flip
WaitKey
End
