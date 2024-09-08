<a name="readme-top"></a>

<!-- PROJECT SHIELDS -->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![LinkedIn][linkedin-shield]][linkedin-url]

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/suny-am/mega">
    <img src=".docs/images/logo.png" alt="Logo" width="80" height="80">
  </a>

<h3 align="center">Mega Render Engine</h3>
  <p align="center">
    <!-- <a href="https://github.com/suny-am/mega"><strong>Explore the docs »</strong></a>
    ·
    <a href="https://github.com/suny-am/mega">View Demo</a>
    · -->
    <a href="https://github.com/suny-am/mega/issues/new?labels=bug&template=bug-report---.md">Report Bug</a>
    ·
    <a href="https://github.com/suny-am/mega/issues/new?labels=enhancement&template=feature-request---.md">Request Feature</a>
  </p>
</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#resources">Resources</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->
## About The Project

<!-- 
[![Product Name Screen Shot][product-screenshot]](https://example.com)
-->

Mega is a WebGPU based (targeting DAWN/WGPU and the web via Emscripten) render engine.

<p align="right"><a href="#readme-top">🔝</a></p>

### Built With

[![CPlusPlus][CPlusPlus]][CPlusPlus-url]
[![CMake][Cmake]][CMake-url]
[![WebGPU][WebGPU]][WebGPU-url]
[![glTF][glTF]][glTF-url]

<p align="right"><a href="#readme-top">🔝</a></p>

<!-- GETTING STARTED -->
## Getting Started

This is an example of how you may give instructions on setting up your project locally.
To get a local copy up and running follow these simple example steps.

### Prerequisites

This is an example of how to list things you need to use the software and how to install them.
The foolowing examples pertains to installing the required dependencies on MacOS.

#### CMake

  ```sh
  # Make sure to install XCode via the App store first
  xcode-select --install && \
  brew install cmake
  ```

### Installation

#### 1. Clone the repo

```sh
git clone https://github.com/suny-am/mega.git && \
cd mega
```

#### 2. Build the project

navigate to the project directory of your choice

```sh
# Working Directory Example
cd 1.getting_started/hello_webgpu
```

Then choose one of three compile options:

##### WGPU-Native

```sh
cmake -B build-wgpu -DWEBGPU_BACKEND=WGPU
cmake --build build-wgpu
```

##### Dawn

```sh
cmake -B build-dawn -DWEBGPU_BACKEND=DAWN
cmake --build build-dawn
```

##### Emscripten

```sh
emcmake cmake -B build-emscripten
cmake --build build-emscripten
```

<p align="right"><a href="#readme-top">🔝</a></p>

<!-- CONTRIBUTING -->
## Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

### 1. Fork the Project

```sh
gh repo fork suny-am/mega --clone
cd mega
```

### 2. Create your Feature Branch

```sh
git checkout -b feature/aNewCoolFeature
```

### 3. Commit your Changes

```sh
`git commit -m 'Add a new cool feature'
```

### 4. Push to the Branch

```sh
git push origin feature/aNewCoolFeature
```

### 5. Open a Pull Request

```sh
gh pr create 
```

<p align="right"><a href="#readme-top">🔝</a></p>

<!-- USAGE EXAMPLES -->
## Usage

Use this space to show useful examples of how a project can be used. Additional screenshots, code examples and demos work well in this space. You may also link to more resources.

TBD

<p align="right"><a href="#readme-top">🔝</a></p>

<!-- ROADMAP -->
## Roadmap

- [ ] File parsing
  - [x] Load .obj
  - [x] Load .gltf/glb
  - [ ] native file dialog
  - [ ] TBD (🚧)
- [ ] Dynamics
  - [x] World color
- [ ] Shaders
  - [x] BRDF
  - [ ] TBD (🚧)
- [ ] UI
  - [x] Dear ImGui integration
  - [ ] ImGuizmo integration
  - [ ] TBD (🚧)
- [ ] Camera control
  - [x] Orbit (turntable)
  - [x] Zoom
  - [ ] Pan
  - [ ] TBD (🚧)
- [ ] TBD (🚧)

<p align="right"><a href="#readme-top">🔝</a></p>

<!-- RESOURCES -->

## Resources

### References

- [W3C WebGPU Reference](https://www.w3.org/TR/webgpu/)
- [W3C WGSL Reference](https://www.w3.org/TR/WGSL/)

### Assets

- [Khronos Group glTF sample assets](https://github.com/KhronosGroup/glTF-Sample-Assets)

### textures

- [cobblestone_floor_08_diff_2k.jpg](https://eliemichel.github.io/LearnWebGPU/_downloads/c69c56204b32f85418889a40235cf7f5/cobblestone_floor_08_diff_2k.jpg)
- [cobblestone_floor_08_nor_gl_2k.png](https://eliemichel.github.io/LearnWebGPU/_downloads/5d69b9dffba8a2649b8c223d042347b7/cobblestone_floor_08_nor_gl_2k.png)

### models

- [plane.obj](https://eliemichel.github.io/LearnWebGPU/_downloads/4336d1767fec66e6d2c5aca98e086357/plane.obj)
- [cylinder.obj](https://eliemichel.github.io/LearnWebGPU/_downloads/a807bbb5c9ad69e555e25d70b1fcf26e/cylinder.obj)
- [fourareen.zip (Scottish Maritim Museum assets)](https://eliemichel.github.io/LearnWebGPU/_downloads/b191c7338d2723dd56474556616f5411/fourareen.zip)

### shaders

<!-- LICENSE -->
## License

Distributed under the MIT License. See [LICENCE.txt](LICENCE.txt) for more information.

<p align="right"><a href="#readme-top">🔝</a></p>

<!-- CONTACT -->
## Contact

Your Name - [@sunyam.bsky.social](https://bsky.app/profile/sunyam.bsky.social) - [visualarea.1@gmail.com](mailto:visualarea.1@gmail.com)

Project Link: [https://github.com/suny-am/mega](https://github.com/suny-am/mega)

<p align="right"><a href="#readme-top">🔝</a></p>

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/suny-am/mega.svg?style=for-the-badge
[contributors-url]: https://github.com/suny-am/mega/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/suny-am/mega?style=for-the-badge
[forks-url]: https://github.com/suny-am/mega/network/members
[stars-shield]: https://img.shields.io/github/stars/suny-am/mega.svg?style=for-the-badge
[stars-url]: https://github.com/suny-am/mega/stargazers
[issues-shield]: https://img.shields.io/github/issues/suny-am/mega.svg?style=for-the-badge
[issues-url]: https://github.com/suny-am/mega/issues
[license-shield]: https://img.shields.io/github/license/suny-am/mega.svg?style=for-the-badge
[license-url]: https://github.com/suny-am/mega/blob/master/LICENSE.txt
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://linkedin.com/in/carl-sandberg-01070a2b6/
[CPlusPlus]: https://img.shields.io/badge/c%2B%2B-00599C?style=for-the-badge&logo=cplusplus
[CPlusPlus-url]: https://cplusplus.com
[CMake]: https://img.shields.io/badge/cmake-064F8C?style=for-the-badge&logo=cmake
[CMake-url]: https://cmake.org
[WebGPU]: https://img.shields.io/badge/webgpu-%23005A9C?style=for-the-badge&logo=webgpu
[WebGPU-url]: https://gpuweb.github.io
[glTF]: https://img.shields.io/badge/gltf-%2387C540?style=for-the-badge&logo=gltf&logoColor=black&logoSize=auto
[glTF-url]:https://www.khronos.org/gltf/
