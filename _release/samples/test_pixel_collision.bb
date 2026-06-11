; Test pixel-perfect collision
; Press arrow keys to move the circle
; The background color changes when collision is detected

Graphics 800, 600, 32, 2

; Create a circle image (with transparency)
circleImg = CreateImage(64, 64)
SetBuffer ImageBuffer(circleImg)
ClsColor 0, 0, 0
Cls
Color 255, 0, 0
Oval 0, 0, 64, 64, True
MaskImage circleImg, 0, 0, 0

; Create a star/cross shape (with transparency)
starImg = CreateImage(64, 64)
SetBuffer ImageBuffer(starImg)
ClsColor 0, 0, 0
Cls
Color 0, 255, 0
; Draw a cross shape
Rect 24, 0, 16, 64, True
Rect 0, 24, 64, 16, True
MaskImage starImg, 0, 0, 0

SetBuffer BackBuffer()

; Position
circleX# = 100.0
circleY# = 300.0
starX = 400
starY = 300

; Instructions
Print "Pixel-Perfect Collision Test"
Print "Use arrow keys to move the red circle"
Print "Green = no collision, Red = collision detected"
Print ""

While Not KeyHit(1) ; ESC to exit

    ; Movement
    If KeyDown(203) Then circleX = circleX - 2 ; Left
    If KeyDown(205) Then circleX = circleX + 2 ; Right
    If KeyDown(200) Then circleY = circleY - 2 ; Up
    If KeyDown(208) Then circleY = circleY + 2 ; Down

    ; Check pixel-perfect collision (solid=False means pixel-perfect)
    collision = ImagesCollide(circleImg, Int(circleX), Int(circleY), 0, starImg, starX, starY, 0)

    ; Clear with color based on collision
    If collision Then
        ClsColor 64, 0, 0 ; Dark red = collision
    Else
        ClsColor 0, 64, 0 ; Dark green = no collision
    EndIf
    Cls

    ; Draw images
    DrawImage starImg, starX, starY
    DrawImage circleImg, Int(circleX), Int(circleY)

    ; Info
    Color 255, 255, 255
    Text 10, 10, "Circle pos: " + Int(circleX) + ", " + Int(circleY)
    Text 10, 30, "Collision: " + collision
    Text 10, 50, "Arrow keys to move, ESC to exit"

    Flip

Wend

End
